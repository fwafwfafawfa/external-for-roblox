#include "esp.hpp"
#include "../game/core.hpp"
#include "../Tphandler.hpp"
#include "../addons/kernel/memory.hpp"
#include "../game/offsets/offsets.hpp"
#include "../handlers/overlay/draw.hpp"
#include "../addons/imgui/imgui.h"
#include "../handlers/utility/utility.hpp"
#include "../../../handlers/vars.hpp"
#include <chrono>
#include <mutex>
#include <thread>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <functional>
#include <unordered_set>

// ==================== STATIC MEMBER DEFINITIONS ====================
std::vector<WorldPart> c_esp::geometry;
double c_esp::last_refresh = 0.0;
std::atomic<bool> c_esp::building{ false };
std::atomic<bool> c_esp::ready{ false };
std::atomic<bool> c_esp::shutdown_requested{ false };  // NEW
std::unordered_map<uintptr_t, std::pair<bool, std::chrono::steady_clock::time_point>> c_esp::vis_cache;
std::mutex c_esp::geometry_mtx;
std::mutex c_esp::vis_cache_mtx;
std::unordered_map<uintptr_t, TargetData> c_esp::target_tracking;
std::mutex c_esp::tracking_mtx;
std::mutex c_esp::players_mtx;
float vars::bss::stored_vicious_x = 0.0f;
float vars::bss::stored_vicious_y = 0.0f;
float vars::bss::stored_vicious_z = 0.0f;


std::atomic<bool> c_esp::hitbox_thread_running{ false };
int c_esp::hitbox_processed_count = 0;

// ==================== NEW: Player cache to reduce memory reads ====================
static std::vector<uintptr_t> s_cached_players;
static std::mutex s_players_cache_mtx;
static std::chrono::steady_clock::time_point s_last_players_update;

// ==================== NEW: Thread-local buffer to avoid string allocations ====================
thread_local static char s_string_buffer[256];

// ==================== HELPER FUNCTIONS ====================
static inline vector normalize_vec(const vector& v)
{
    float mag = sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
    if (mag < 1e-6f) return { 0.f, 0.f, 0.f };
    float inv_mag = 1.0f / mag;  // OPTIMIZATION: multiply is faster than divide
    return { v.x * inv_mag, v.y * inv_mag, v.z * inv_mag };
}

static inline float magnitude_vec(const vector& v)
{
    return sqrtf(v.x * v.x + v.y * v.y + v.z * v.z);
}

static inline bool check_ray_box(const vector& origin, const vector& dir, const WorldPart& part, float dist)
{
    vector to_part{ part.pos.x - origin.x, part.pos.y - origin.y, part.pos.z - origin.z };
    float radius = sqrtf(part.size.x * part.size.x + part.size.y * part.size.y + part.size.z * part.size.z) * 0.866f;
    float dist_sq = to_part.x * to_part.x + to_part.y * to_part.y + to_part.z * to_part.z;

    if (dist_sq > (radius + dist) * (radius + dist))
        return false;

    vector local_o{ origin.x - part.pos.x, origin.y - part.pos.y, origin.z - part.pos.z };
    vector half{ part.size.x * 0.5f, part.size.y * 0.5f, part.size.z * 0.5f };

    float tmin = -FLT_MAX, tmax = FLT_MAX;

    // OPTIMIZATION: Use arrays instead of repeated if/else
    float coords_o[3] = { local_o.x, local_o.y, local_o.z };
    float coords_d[3] = { dir.x, dir.y, dir.z };
    float coords_h[3] = { half.x, half.y, half.z };

    for (int i = 0; i < 3; i++)
    {
        float o = coords_o[i];
        float d = coords_d[i];
        float h = coords_h[i];

        if (std::fabs(d) < 1e-6f)
        {
            if (o < -h || o > h)
                return false;
        }
        else
        {
            float inv_d = 1.0f / d;  // OPTIMIZATION: compute once
            float t1 = (-h - o) * inv_d;
            float t2 = (h - o) * inv_d;
            if (t1 > t2) std::swap(t1, t2);
            if (t1 > tmin) tmin = t1;
            if (t2 < tmax) tmax = t2;
            if (tmin > tmax || tmax < 0.0f || tmin > dist)
                return false;
        }
    }

    return (tmin > 0.0f || tmax > 0.0f) && tmin <= dist && tmin <= tmax;
}

static inline bool is_valid_pointer(uintptr_t ptr)
{
    if (!ptr) return false;
    if (ptr < 0x10000) return false;
    if (ptr > 0x7FFFFFFFFFFF) return false;
    return true;
}

// ==================== NEW: Cached player list getter ====================
static std::vector<uintptr_t> get_cached_players(uintptr_t datamodel)
{
    auto now = std::chrono::steady_clock::now();

    std::lock_guard<std::mutex> lock(s_players_cache_mtx);

    // Only refresh every 100ms
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - s_last_players_update).count();
    if (elapsed > 100 || s_cached_players.empty())
    {
        s_cached_players = core.get_players(datamodel);
        s_last_players_update = now;
    }

    return s_cached_players;
}

// ==================== NEW: Shutdown function to clean up ====================
void c_esp::shutdown()
{
    shutdown_requested = true;

    // Wait for building thread to finish (max 5 seconds)
    int timeout = 50;
    while (building && timeout > 0)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        timeout--;
    }

    // Clear all caches to free memory
    {
        std::lock_guard<std::mutex> lk(geometry_mtx);
        geometry.clear();
        geometry.shrink_to_fit();  // Actually release the memory
    }
    {
        std::lock_guard<std::mutex> lk(vis_cache_mtx);
        vis_cache.clear();
    }
    {
        std::lock_guard<std::mutex> lk(tracking_mtx);
        target_tracking.clear();
    }
    {
        std::lock_guard<std::mutex> lk(s_players_cache_mtx);
        s_cached_players.clear();
        s_cached_players.shrink_to_fit();
    }
}

// ==================== NEW: Visibility cache cleanup ====================
void c_esp::cleanup_vis_cache()
{
    static std::chrono::steady_clock::time_point last_cleanup;
    auto now = std::chrono::steady_clock::now();

    // Only cleanup every 5 seconds to avoid overhead
    if (std::chrono::duration_cast<std::chrono::seconds>(now - last_cleanup).count() < 5)
        return;

    last_cleanup = now;

    std::lock_guard<std::mutex> vlk(vis_cache_mtx);

    // FIX: Hard limit on cache size to prevent memory leak
    constexpr size_t MAX_CACHE_SIZE = 200;

    if (vis_cache.size() > MAX_CACHE_SIZE)
    {
        // Remove half the cache when it gets too big
        auto it = vis_cache.begin();
        size_t to_remove = vis_cache.size() / 2;
        for (size_t i = 0; i < to_remove && it != vis_cache.end(); i++)
        {
            it = vis_cache.erase(it);
        }
    }

    // Remove stale entries (older than 1 second)
    for (auto it = vis_cache.begin(); it != vis_cache.end();)
    {
        if (std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second.second).count() > 1000)
            it = vis_cache.erase(it);
        else
            ++it;
    }
}

void c_esp::calculate_fps()
{
    // Empty - functionality moved inline where needed
}

// ==================== FIXED: update_world_cache ====================
void c_esp::update_world_cache()
{
    if (building || shutdown_requested) return;  // FIX: Check shutdown
    building = true;

    std::thread([]()
        {
            std::vector<WorldPart> parts;
            parts.reserve(3000);

            // FIX: Check shutdown throughout
            if (shutdown_requested)
            {
                building = false;
                return;
            }

            uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
            if (!workspace)
            {
                std::lock_guard<std::mutex> lk(c_esp::geometry_mtx);
                c_esp::geometry.clear();
                ready = true;
                building = false;
                return;
            }

            std::vector<uintptr_t> players = core.get_players(g_main::datamodel);

            // FIX: Use unordered_set for O(1) lookup instead of O(n) vector search
            std::unordered_set<uintptr_t> player_models;
            player_models.reserve(players.size());
            for (auto& p : players)
            {
                auto m = core.get_model_instance(p);
                if (m) player_models.insert(m);
            }

            // FIX: Use iterative approach instead of recursive to prevent stack overflow
            std::vector<uintptr_t> stack;
            stack.reserve(1000);
            stack.push_back(workspace);

            while (!stack.empty() && !shutdown_requested)
            {
                uintptr_t parent = stack.back();
                stack.pop_back();

                auto children = core.children(parent);
                for (auto child : children)
                {
                    if (!child || shutdown_requested) continue;

                    std::string class_name = core.get_instance_classname(child);

                    if (class_name == "Part" || class_name == "MeshPart" || class_name == "UnionOperation")
                    {
                        bool is_player_part = false;

                        // Check if this part belongs to a player (with depth limit)
                        uintptr_t check = child;
                        int depth = 0;
                        while (check != 0 && depth < 10)  // FIX: Limit depth to prevent infinite loop
                        {
                            if (player_models.count(check))
                            {
                                is_player_part = true;
                                break;
                            }
                            check = memory->read<uintptr_t>(check + offsets::Parent);
                            depth++;
                        }

                        if (!is_player_part)
                        {
                            uintptr_t primitive = memory->read<uintptr_t>(child + offsets::Primitive);
                            if (primitive >= 0x10000)
                            {
                                float transparency = memory->read<float>(primitive + offsets::Transparency);
                                if (transparency <= 0.9f)
                                {
                                    vector size = memory->read<vector>(primitive + offsets::PartSize);
                                    float vol = size.x * size.y * size.z;
                                    if (vol >= 0.5f && vol <= 8000000.0f)
                                    {
                                        vector pos = memory->read<vector>(primitive + offsets::Position);
                                        parts.push_back({ pos, size, vol, vol > 10.0f });
                                    }
                                }
                            }
                        }
                    }

                    // Add children to stack for processing
                    stack.push_back(child);
                }
            }

            if (!shutdown_requested)
            {
                {
                    std::lock_guard<std::mutex> lk(c_esp::geometry_mtx);
                    c_esp::geometry = std::move(parts);
                }

                {
                    std::lock_guard<std::mutex> lk2(c_esp::vis_cache_mtx);
                    c_esp::vis_cache.clear();
                }

                ready = true;
            }

            building = false;

        }).detach();
}

// ==================== FIXED: is_visible ====================
bool c_esp::is_visible(
    const vector& from,
    const vector& head,
    const vector& torso,
    const vector& pelvis,
    const vector& left_foot,
    const vector& right_foot,
    uintptr_t target_model
)
{
    if (!ready) return false;

    std::vector<WorldPart> local_geometry;
    {
        std::lock_guard<std::mutex> lk(geometry_mtx);
        local_geometry = geometry;
    }

    if (local_geometry.empty()) return false;

    // Cache check
    if (target_model != 0)
    {
        std::lock_guard<std::mutex> vlk(vis_cache_mtx);
        auto it = vis_cache.find(target_model);
        if (it != vis_cache.end())
        {
            auto age = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - it->second.second).count();
            if (age < 50) return it->second.first;
        }
    }

    struct Ray
    {
        vector target;
        vector dir;
        float dist;
    };

    Ray rays[5] = {
        { head, normalize_vec({ head.x - from.x, head.y - from.y, head.z - from.z }), magnitude_vec({ head.x - from.x, head.y - from.y, head.z - from.z }) },
        { torso, normalize_vec({ torso.x - from.x, torso.y - from.y, torso.z - from.z }), magnitude_vec({ torso.x - from.x, torso.y - from.y, torso.z - from.z }) },
        { pelvis, normalize_vec({ pelvis.x - from.x, pelvis.y - from.y, pelvis.z - from.z }), magnitude_vec({ pelvis.x - from.x, pelvis.y - from.y, pelvis.z - from.z }) },
        { left_foot, normalize_vec({ left_foot.x - from.x, left_foot.y - from.y, left_foot.z - from.z }), magnitude_vec({ left_foot.x - from.x, left_foot.y - from.y, left_foot.z - from.z }) },
        { right_foot, normalize_vec({ right_foot.x - from.x, right_foot.y - from.y, right_foot.z - from.z }), magnitude_vec({ right_foot.x - from.x, right_foot.y - from.y, right_foot.z - from.z }) }
    };

    std::sort(std::begin(rays), std::end(rays), [](const Ray& a, const Ray& b) { return a.dist < b.dist; });

    int visible_count = 0;
    int checked = 0;

    for (const auto& ray : rays)
    {
        if (checked >= 5) break;
        if (ray.dist <= 0.0f)
        {
            checked++;
            continue;
        }

        bool clear = true;
        for (const auto& part : local_geometry)
        {
            if (!part.large && part.vol < 1.0f) continue;
            if (part.vol <= 0.0f || part.size.x <= 0.0f || part.size.y <= 0.0f || part.size.z <= 0.0f) continue;

            if (check_ray_box(from, ray.dir, part, ray.dist))
            {
                float part_dist = magnitude_vec({ part.pos.x - from.x, part.pos.y - from.y, part.pos.z - from.z });
                if (std::fabs(part_dist - ray.dist) < 1.5f) continue;
                clear = false;
                break;
            }
        }

        if (clear && ++visible_count >= 2)
        {
            if (target_model != 0)
            {
                std::lock_guard<std::mutex> vlk(vis_cache_mtx);
                vis_cache[target_model] = { true, std::chrono::steady_clock::now() };
            }
            return true;
        }
        checked++;
    }

    bool result = visible_count >= 2;

    if (target_model != 0)
    {
        std::lock_guard<std::mutex> vlk(vis_cache_mtx);
        vis_cache[target_model] = { result, std::chrono::steady_clock::now() };
    }

    // FIX: Call cleanup periodically instead of inline
    cleanup_vis_cache();

    return result;
}

// ==================== FIXED: run_players ====================
void c_esp::run_players(view_matrix_t viewmatrix)
{
    if (vars::esp::show_fps)
    {
        uintptr_t task_scheduler = memory->read<uintptr_t>(offsets::TaskSchedulerPointer);
        if (task_scheduler)
        {
            float fps_cap = memory->read<float>(task_scheduler + offsets::TaskSchedulerMaxFPS);
            snprintf(s_string_buffer, sizeof(s_string_buffer), "FPS: %.0f", fps_cap);
            draw.outlined_string(ImVec2(10, 10), s_string_buffer, ImColor(0, 255, 0, 255), ImColor(0, 0, 0, 255), false);
        }
    }

    std::vector<uintptr_t> players = get_cached_players(g_main::datamodel);

    const int screen_width = core.get_screen_width();
    const int screen_height = core.get_screen_height();

    for (auto& player : players)
    {
        if (!player || player == g_main::localplayer)
            continue;

        if (vars::esp::hide_teammates)
        {
            uintptr_t player_team = memory->read<uintptr_t>(player + offsets::Player::Team);
            if (player_team == g_main::localplayer_team && player_team != 0)
                continue;
        }

        auto model = core.get_model_instance(player);
        if (!model)
            continue;

        auto humanoid = core.find_first_child_class(model, "Humanoid");
        if (!humanoid)
            continue;

        float health = memory->read<float>(humanoid + offsets::Health);
        float max_health = memory->read<float>(humanoid + offsets::MaxHealth);

        if (vars::esp::hide_dead && health <= 0.0f)
            continue;

        if (health <= 0.0f || max_health <= 0.0f)
            continue;

        auto player_root = core.find_first_child(model, "HumanoidRootPart");
        if (!player_root)
            continue;

        auto p_player_root = memory->read<uintptr_t>(player_root + offsets::Primitive);
        if (!p_player_root)
            continue;

        std::string player_name = core.get_instance_name(player);
        if (player_name.empty())
            continue;

        vector w_player_root = memory->read<vector>(p_player_root + offsets::Position);

        float hip_height = memory->read<float>(humanoid + offsets::HipHeight);
        if (hip_height <= 0.0f || hip_height > 10.0f)
            hip_height = 2.0f;

        float height_above_hrp = 2.5f;
        float height_below_hrp = hip_height + 0.5f;

        vector w_head_top;
        w_head_top.x = w_player_root.x;
        w_head_top.y = w_player_root.y + height_above_hrp;
        w_head_top.z = w_player_root.z;

        vector w_foot_pos;
        w_foot_pos.x = w_player_root.x;
        w_foot_pos.y = w_player_root.y - height_below_hrp;
        w_foot_pos.z = w_player_root.z;

        vector2d s_head_top;
        if (!core.world_to_screen(w_head_top, s_head_top, viewmatrix))
            continue;

        vector2d s_foot_pos;
        if (!core.world_to_screen(w_foot_pos, s_foot_pos, viewmatrix))
            continue;

        float height = s_foot_pos.y - s_head_top.y;

        if (height <= 5.0f || height > 1000.0f)
            continue;

        if (s_head_top.x < 0 || s_head_top.x > screen_width || s_head_top.y < 0 || s_head_top.y > screen_height)
            continue;

        if (s_foot_pos.x < 0 || s_foot_pos.x > screen_width || s_foot_pos.y < 0 || s_foot_pos.y > screen_height)
            continue;

        float width = height * 0.5f;
        float center_x = s_head_top.x;

        ImVec2 top_left = ImVec2(center_x - (width * 0.5f), s_head_top.y);
        ImVec2 bottom_right = ImVec2(center_x + (width * 0.5f), s_foot_pos.y);

        if (vars::esp::show_box)
        {
            draw.outlined_rectangle(top_left, bottom_right, vars::esp::esp_box_color, ImColor(0, 0, 0, 255), 1.0f);
        }

        if (vars::esp::show_tracers)
        {
            draw.line(
                ImVec2(screen_width * 0.5f, (float)screen_height),
                ImVec2(center_x, s_foot_pos.y),
                vars::esp::esp_tracer_color,
                1.0f
            );
        }

        float health_percent = fmaxf(fminf(health / max_health, 1.0f), 0.0f);
        ImColor health_color = ImColor(
            static_cast<int>(255 * (1.0f - health_percent)),
            static_cast<int>(255 * health_percent),
            0, 255
        );

        float text_y = top_left.y - 15.0f;
        draw.outlined_string(ImVec2(center_x, text_y), player_name.c_str(), vars::esp::esp_name_color, ImColor(0, 0, 0, 255), true);

        if (vars::esp::show_weapon)
        {
            auto equipped_tool = core.find_first_child_class(model, "Tool");
            if (equipped_tool)
            {
                std::string weapon_name = core.get_instance_name(equipped_tool);
                if (!weapon_name.empty())
                {
                    text_y -= 15.0f;
                    draw.outlined_string(ImVec2(center_x, text_y), weapon_name.c_str(), ImColor(255, 165, 0, 255), ImColor(0, 0, 0, 255), true);
                }
            }
        }

        if (vars::esp::show_health)
        {
            snprintf(s_string_buffer, sizeof(s_string_buffer), "[HP: %.0f]", health);
            draw.outlined_string(ImVec2(center_x, s_foot_pos.y + 5.0f), s_string_buffer, health_color, ImColor(0, 0, 0, 255), true);
        }

        if (vars::esp::show_distance)
        {
            uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
            uintptr_t local_char = core.find_first_child(workspace, core.get_instance_name(g_main::localplayer));

            if (local_char)
            {
                auto local_root = core.find_first_child(local_char, "HumanoidRootPart");
                if (local_root)
                {
                    auto p_local_root = memory->read<uintptr_t>(local_root + offsets::Primitive);
                    if (p_local_root)
                    {
                        vector local_pos = memory->read<vector>(p_local_root + offsets::Position);
                        float dx = w_player_root.x - local_pos.x;
                        float dy = w_player_root.y - local_pos.y;
                        float dz = w_player_root.z - local_pos.z;
                        float distance = sqrtf(dx * dx + dy * dy + dz * dz);

                        snprintf(s_string_buffer, sizeof(s_string_buffer), "[%.0fm]", distance);
                        draw.outlined_string(ImVec2(center_x, s_foot_pos.y + 20.0f), s_string_buffer, vars::esp::esp_distance_color, ImColor(0, 0, 0, 255), true);
                    }
                }
            }
        }

        if (vars::esp::show_skeleton)
        {
            DrawSkeleton(model, viewmatrix);
        }
    }
}
void c_esp::DrawSkeleton(uintptr_t model, view_matrix_t viewmatrix)
{
    bool is_r15 = (core.find_first_child(model, "UpperTorso") != 0);

    auto get_bone_pos = [&](const char* name, vector2d& out) -> bool {
        uintptr_t bone = core.find_first_child(model, name);
        if (!bone) return false;

        uintptr_t prim = memory->read<uintptr_t>(bone + offsets::Primitive);
        if (!prim) return false;

        vector world = memory->read<vector>(prim + offsets::Position);
        return core.world_to_screen(world, out, viewmatrix);
        };

    if (is_r15)
    {
        vector2d head, upper_torso, lower_torso;
        vector2d left_upper_arm, left_lower_arm, left_hand;
        vector2d right_upper_arm, right_lower_arm, right_hand;
        vector2d left_upper_leg, left_lower_leg, left_foot;
        vector2d right_upper_leg, right_lower_leg, right_foot;

        bool has_head = get_bone_pos("Head", head);
        bool has_upper = get_bone_pos("UpperTorso", upper_torso);
        bool has_lower = get_bone_pos("LowerTorso", lower_torso);

        get_bone_pos("LeftUpperArm", left_upper_arm);
        get_bone_pos("LeftLowerArm", left_lower_arm);
        get_bone_pos("LeftHand", left_hand);
        get_bone_pos("RightUpperArm", right_upper_arm);
        get_bone_pos("RightLowerArm", right_lower_arm);
        get_bone_pos("RightHand", right_hand);
        get_bone_pos("LeftUpperLeg", left_upper_leg);
        get_bone_pos("LeftLowerLeg", left_lower_leg);
        get_bone_pos("LeftFoot", left_foot);
        get_bone_pos("RightUpperLeg", right_upper_leg);
        get_bone_pos("RightLowerLeg", right_lower_leg);
        get_bone_pos("RightFoot", right_foot);

        ImColor col = vars::esp::esp_skeleton_color;
        float thickness = 1.5f;

        if (has_head && has_upper) draw.line(ImVec2(head.x, head.y), ImVec2(upper_torso.x, upper_torso.y), col, thickness);
        if (has_upper && has_lower) draw.line(ImVec2(upper_torso.x, upper_torso.y), ImVec2(lower_torso.x, lower_torso.y), col, thickness);

        draw.line(ImVec2(upper_torso.x, upper_torso.y), ImVec2(left_upper_arm.x, left_upper_arm.y), col, thickness);
        draw.line(ImVec2(left_upper_arm.x, left_upper_arm.y), ImVec2(left_lower_arm.x, left_lower_arm.y), col, thickness);
        draw.line(ImVec2(left_lower_arm.x, left_lower_arm.y), ImVec2(left_hand.x, left_hand.y), col, thickness);

        draw.line(ImVec2(upper_torso.x, upper_torso.y), ImVec2(right_upper_arm.x, right_upper_arm.y), col, thickness);
        draw.line(ImVec2(right_upper_arm.x, right_upper_arm.y), ImVec2(right_lower_arm.x, right_lower_arm.y), col, thickness);
        draw.line(ImVec2(right_lower_arm.x, right_lower_arm.y), ImVec2(right_hand.x, right_hand.y), col, thickness);

        draw.line(ImVec2(lower_torso.x, lower_torso.y), ImVec2(left_upper_leg.x, left_upper_leg.y), col, thickness);
        draw.line(ImVec2(left_upper_leg.x, left_upper_leg.y), ImVec2(left_lower_leg.x, left_lower_leg.y), col, thickness);
        draw.line(ImVec2(left_lower_leg.x, left_lower_leg.y), ImVec2(left_foot.x, left_foot.y), col, thickness);

        draw.line(ImVec2(lower_torso.x, lower_torso.y), ImVec2(right_upper_leg.x, right_upper_leg.y), col, thickness);
        draw.line(ImVec2(right_upper_leg.x, right_upper_leg.y), ImVec2(right_lower_leg.x, right_lower_leg.y), col, thickness);
        draw.line(ImVec2(right_lower_leg.x, right_lower_leg.y), ImVec2(right_foot.x, right_foot.y), col, thickness);
    }
    else
    {
        vector2d head, torso, left_arm, right_arm, left_leg, right_leg;

        get_bone_pos("Head", head);
        get_bone_pos("Torso", torso);
        get_bone_pos("Left Arm", left_arm);
        get_bone_pos("Right Arm", right_arm);
        get_bone_pos("Left Leg", left_leg);
        get_bone_pos("Right Leg", right_leg);

        ImColor col = vars::esp::esp_skeleton_color;
        float thickness = 1.5f;

        draw.line(ImVec2(head.x, head.y), ImVec2(torso.x, torso.y), col, thickness);
        draw.line(ImVec2(torso.x, torso.y), ImVec2(left_arm.x, left_arm.y), col, thickness);
        draw.line(ImVec2(torso.x, torso.y), ImVec2(right_arm.x, right_arm.y), col, thickness);
        draw.line(ImVec2(torso.x, torso.y), ImVec2(left_leg.x, left_leg.y), col, thickness);
        draw.line(ImVec2(torso.x, torso.y), ImVec2(right_leg.x, right_leg.y), col, thickness);
    }
}

// ==================== UNCHANGED: run_aimbot (your original with minor fixes) ====================
void c_esp::run_aimbot(view_matrix_t viewmatrix)
{
    if (!g_main::datamodel || !g_main::localplayer) return;

    // Cache update
    static auto last_cache_update = std::chrono::high_resolution_clock::now();
    auto now_time = std::chrono::high_resolution_clock::now();
    if (std::chrono::duration_cast<std::chrono::seconds>(now_time - last_cache_update).count() >= 5)
    {
        update_world_cache();
        last_cache_update = now_time;
    }

    // FIX: Use cached players
    std::vector<uintptr_t> players = get_cached_players(g_main::datamodel);

    vector2d crosshair_pos = {
        static_cast<float>(core.get_screen_width() / 2),
        static_cast<float>(core.get_screen_height() / 2)
    };

    if (vars::aimbot::show_fov_circle)
    {
        draw.circle(ImVec2(crosshair_pos.x, crosshair_pos.y), vars::aimbot::fov, ImColor(255, 255, 255, 255), 1.0f);
    }

    bool aimbot_active = vars::aimbot::toggled && GetAsyncKeyState(vars::aimbot::activation_key);
    bool triggerbot_active = vars::triggerbot::toggled && GetAsyncKeyState(vars::triggerbot::activation_key);

    if (!aimbot_active && !triggerbot_active)
    {
        this->leftover_x = 0.0f;
        this->leftover_y = 0.0f;
        this->smoothed_delta_x = 0.0f;
        this->smoothed_delta_y = 0.0f;
        this->locked_target = 0;
        this->last_target_pos = { 0, 0, 0 };
        return;
    }

    uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
    uintptr_t camera = core.find_first_child_class(workspace, "Camera");
    vector cam_pos = {};
    if (camera)
        cam_pos = memory->read<vector>(camera + offsets::CameraPos);

    uintptr_t current_target = 0;
    uintptr_t current_target_model = 0;

    // STICKY AIM
    if (vars::aimbot::sticky_aim && this->locked_target != 0 && aimbot_active)
    {
        bool locked_target_still_valid = false;
        for (auto& player : players)
        {
            if (player == this->locked_target)
            {
                if (vars::aimbot::ignore_teammates)
                {
                    uintptr_t player_team = memory->read<uintptr_t>(player + offsets::Player::Team);
                    if (player_team == g_main::localplayer_team && player_team != 0)
                    {
                        this->locked_target = 0;
                        break;
                    }
                }

                auto model = core.get_model_instance(player);
                if (model)
                {
                    auto humanoid = core.find_first_child_class(model, "Humanoid");
                    if (humanoid)
                    {
                        float health = memory->read<float>(humanoid + offsets::Health);

                        if (vars::aimbot::unlock_on_death && health <= 0.0f)
                        {
                            this->locked_target = 0;
                            break;
                        }

                        if (health > 0.0f)
                        {
                            const char* target_bone_name = get_target_bone_name(model, player);
                            auto target_bone = core.find_first_child(model, target_bone_name);

                            if (target_bone)
                            {
                                auto p_target_bone = memory->read<uintptr_t>(target_bone + offsets::Primitive);
                                if (p_target_bone)
                                {
                                    vector w_target_bone_pos = memory->read<vector>(p_target_bone + offsets::Position);
                                    vector2d s_target_bone_pos;

                                    if (core.world_to_screen(w_target_bone_pos, s_target_bone_pos, viewmatrix))
                                    {
                                        locked_target_still_valid = true;
                                        current_target = this->locked_target;
                                        current_target_model = model;
                                    }
                                }
                            }
                        }
                    }
                }
                break;
            }
        }

        if (!locked_target_still_valid)
            this->locked_target = 0;
    }

    // FIND NEW TARGET
    if (this->locked_target == 0 || !vars::aimbot::sticky_aim)
    {
        uintptr_t new_closest_player = 0;
        uintptr_t new_closest_model = 0;
        float new_closest_distance = FLT_MAX;
        float search_fov = aimbot_active ? vars::aimbot::fov : vars::triggerbot::fov;

        vector local_player_pos = { 0, 0, 0 };
        uintptr_t local_character_model = core.find_first_child(core.find_first_child_class(g_main::datamodel, "Workspace"), core.get_instance_name(g_main::localplayer));
        if (local_character_model)
        {
            auto local_root = core.find_first_child(local_character_model, "HumanoidRootPart");
            if (local_root)
            {
                auto p_local_root = memory->read<uintptr_t>(local_root + offsets::Primitive);
                if (p_local_root) local_player_pos = memory->read<vector>(p_local_root + offsets::Position);
            }
        }

        for (auto player : players)
        {
            if (!player || player == g_main::localplayer) continue;

            if (vars::aimbot::sticky_aim && this->locked_target != 0 && player != this->locked_target)
                continue;

            if (vars::aimbot::ignore_teammates)
            {
                uintptr_t player_team = memory->read<uintptr_t>(player + offsets::Player::Team);
                if (player_team == g_main::localplayer_team && player_team != 0) continue;
            }

            auto model = core.get_model_instance(player);
            if (!model) continue;

            auto humanoid = core.find_first_child_class(model, "Humanoid");
            if (!humanoid) continue;

            float health = memory->read<float>(humanoid + offsets::Health);
            if (health <= 0.0f) continue;

            auto player_root = core.find_first_child(model, "HumanoidRootPart");
            if (!player_root) continue;

            auto p_player_root = memory->read<uintptr_t>(player_root + offsets::Primitive);
            if (!p_player_root) continue;

            const char* target_bone_name = get_target_bone_name(model, player);
            auto target_bone = core.find_first_child(model, target_bone_name);
            if (!target_bone) continue;

            auto p_target_bone = memory->read<uintptr_t>(target_bone + offsets::Primitive);
            if (!p_target_bone) continue;

            vector w_target_bone_pos = memory->read<vector>(p_target_bone + offsets::Position);
            vector2d s_target_bone_pos;
            if (!core.world_to_screen(w_target_bone_pos, s_target_bone_pos, viewmatrix)) continue;

            float fov_distance = sqrtf(powf(s_target_bone_pos.x - crosshair_pos.x, 2) + powf(s_target_bone_pos.y - crosshair_pos.y, 2));
            if (fov_distance > search_fov) continue;

            float world_distance = sqrtf(powf(w_target_bone_pos.x - local_player_pos.x, 2) +
                powf(w_target_bone_pos.y - local_player_pos.y, 2) +
                powf(w_target_bone_pos.z - local_player_pos.z, 2));

            if (world_distance < new_closest_distance)
            {
                new_closest_distance = world_distance;
                new_closest_player = player;
                new_closest_model = model;
            }
        }

        if (new_closest_player)
        {
            current_target = new_closest_player;
            current_target_model = new_closest_model;
            if (aimbot_active && !this->locked_target) this->locked_target = new_closest_player;
        }
    }

    // AIM AT TARGET
    if (current_target && current_target_model)
    {
        auto model = current_target_model;
        auto player_root = core.find_first_child(model, "HumanoidRootPart");
        auto p_player_root = memory->read<uintptr_t>(player_root + offsets::Primitive);
        vector v_player_root = memory->read<vector>(p_player_root + offsets::Velocity);

        bool is_jumping = false;
        if (vars::aimbot::air_part_enabled)
        {
            is_jumping = v_player_root.y > 5.0f;
        }

        const char* target_bone_name;
        if (is_jumping && vars::aimbot::air_part_enabled)
        {
            target_bone_name = (vars::aimbot::air_part_hitbox == 0) ? "Head" : "HumanoidRootPart";
        }
        else
        {
            target_bone_name = get_target_bone_name(model, current_target);
        }

        auto target_bone = core.find_first_child(model, target_bone_name);
        if (!target_bone)
        {
            reset_aim_state();
            return;
        }

        auto p_target_bone = memory->read<uintptr_t>(target_bone + offsets::Primitive);
        if (!p_target_bone)
        {
            reset_aim_state();
            return;
        }

        vector w_target_bone_pos = memory->read<vector>(p_target_bone + offsets::Position);

        // ANTI-FLICK
        if (vars::aimbot::anti_flick && this->last_target_pos.x != 0)
        {
            float jump_distance = sqrtf(
                powf(w_target_bone_pos.x - this->last_target_pos.x, 2) +
                powf(w_target_bone_pos.y - this->last_target_pos.y, 2) +
                powf(w_target_bone_pos.z - this->last_target_pos.z, 2)
            );

            if (jump_distance > vars::aimbot::anti_flick_distance)
            {
                this->locked_target = 0;
                this->last_target_pos = w_target_bone_pos;
                reset_aim_state();
                return;
            }
        }
        this->last_target_pos = w_target_bone_pos;

        // PREDICTION
        if (vars::aimbot::prediction)
        {
            w_target_bone_pos = predict_position(w_target_bone_pos, v_player_root, cam_pos);
        }

        // SHAKE
        if (vars::aimbot::shake)
        {
            static std::random_device rd;
            static std::mt19937 gen(rd());
            static std::uniform_real_distribution<float> dist(-1.0f, 1.0f);

            w_target_bone_pos.x += dist(gen) * vars::aimbot::shake_x * 0.1f;
            w_target_bone_pos.y += dist(gen) * vars::aimbot::shake_y * 0.1f;
        }

        vector2d s_target_bone_pos;
        if (core.world_to_screen(w_target_bone_pos, s_target_bone_pos, viewmatrix))
        {
            float delta_x = s_target_bone_pos.x - crosshair_pos.x;
            float delta_y = s_target_bone_pos.y - crosshair_pos.y;

            if (triggerbot_active)
            {
                run_triggerbot(model, p_player_root, delta_x, delta_y, cam_pos, w_target_bone_pos);
            }

            if (aimbot_active)
            {
                perform_aim(delta_x, delta_y, s_target_bone_pos.x, s_target_bone_pos.y);
            }
        }
        else
        {
            reset_aim_state();
        }
    }
    else
    {
        reset_aim_state();
    }
}

// ==================== YOUR REMAINING FUNCTIONS (unchanged) ====================

const char* c_esp::get_target_bone_name(uintptr_t model, uintptr_t player)
{
    switch (vars::aimbot::aimbot_hitbox)
    {
    case 0: return "Head";
    case 1: return "HumanoidRootPart";
    case 2: return get_closest_part_name(model);
    case 3: return get_random_part_name();
    default: return "Head";
    }
}

const char* c_esp::get_closest_part_name(uintptr_t model)
{
    if (!model) return "Head";
    const char* parts[] = { "Head", "HumanoidRootPart", "UpperTorso", "LowerTorso", "LeftHand", "RightHand", "LeftFoot", "RightFoot" };
    const char* fallback_parts[] = { "Head", "HumanoidRootPart", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg" };

    POINT p;
    GetCursorPos(&p);
    HWND rw = FindWindowA(nullptr, "Roblox");
    if (rw) ScreenToClient(rw, &p);

    float best_dist = FLT_MAX;
    const char* best_part = "Head";

    for (int i = 0; i < 8; i++)
    {
        auto part = core.find_first_child(model, parts[i]);
        if (!part) continue;

        auto p_part = memory->read<uintptr_t>(part + offsets::Primitive);
        if (!p_part) continue;

        vector w_pos = memory->read<vector>(p_part + offsets::Position);
        vector2d s_pos;

        view_matrix_t vm = memory->read<view_matrix_t>(g_main::v_engine + offsets::viewmatrix);
        if (!core.world_to_screen(w_pos, s_pos, vm)) continue;

        float dx = p.x - s_pos.x;
        float dy = p.y - s_pos.y;
        float dist = dx * dx + dy * dy;

        if (dist < best_dist)
        {
            best_dist = dist;
            best_part = parts[i];
        }
    }

    if (best_dist == FLT_MAX)
    {
        for (int i = 0; i < 7; i++)
        {
            auto part = core.find_first_child(model, fallback_parts[i]);
            if (!part) continue;

            auto p_part = memory->read<uintptr_t>(part + offsets::Primitive);
            if (!p_part) continue;

            vector w_pos = memory->read<vector>(p_part + offsets::Position);
            vector2d s_pos;

            view_matrix_t vm = memory->read<view_matrix_t>(g_main::v_engine + offsets::viewmatrix);
            if (!core.world_to_screen(w_pos, s_pos, vm)) continue;

            float dx = p.x - s_pos.x;
            float dy = p.y - s_pos.y;
            float dist = dx * dx + dy * dy;

            if (dist < best_dist)
            {
                best_dist = dist;
                best_part = fallback_parts[i];
            }
        }
    }

    return best_part;
}

const char* c_esp::get_random_part_name()
{
    const char* parts[] = { "Head", "HumanoidRootPart", "UpperTorso", "LowerTorso" };
    return parts[rand() % 4];
}

vector c_esp::predict_position(vector current_pos, vector velocity, vector cam_pos)
{
    vector predicted = current_pos;

    if (isnan(velocity.x) || isnan(velocity.y) || isnan(velocity.z))
        return current_pos;

    if (vars::aimbot::prediction_x <= 0.0f)
        return current_pos;

    float dx = current_pos.x - cam_pos.x;
    float dy = current_pos.y - cam_pos.y;
    float dz = current_pos.z - cam_pos.z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);

    float distance_scale = distance / 50.0f;
    distance_scale = fmaxf(0.5f, fminf(distance_scale, 2.0f));

    predicted.x += (velocity.x / vars::aimbot::prediction_x) * distance_scale;
    predicted.z += (velocity.z / vars::aimbot::prediction_x) * distance_scale;

    if (!vars::aimbot::prediction_ignore_y)
    {
        predicted.y += (velocity.y / vars::aimbot::prediction_y) * distance_scale;
    }

    return predicted;
}

void c_esp::perform_aim(float delta_x, float delta_y, float target_x, float target_y)
{
    float base_smooth = vars::aimbot::smoothing_factor;
    float eased_smooth = apply_easing(base_smooth, vars::aimbot::smoothing_style);

    this->smoothed_delta_x = (delta_x * eased_smooth) + (this->smoothed_delta_x * (1.0f - eased_smooth));
    this->smoothed_delta_y = (delta_y * eased_smooth) + (this->smoothed_delta_y * (1.0f - eased_smooth));

    if (abs(this->smoothed_delta_x) < vars::aimbot::deadzone && abs(this->smoothed_delta_y) < vars::aimbot::deadzone)
    {
        this->leftover_x = 0.0f;
        this->leftover_y = 0.0f;
        return;
    }

    float aim_delta_x = this->smoothed_delta_x / vars::aimbot::speed;
    float aim_delta_y = this->smoothed_delta_y / vars::aimbot::speed;

    if (vars::aimbot::use_set_cursor_pos)
    {
        int tx = static_cast<int>(target_x);
        int ty = static_cast<int>(target_y);
        POINT current_mouse_pos;
        GetCursorPos(&current_mouse_pos);
        float smooth_factor = 1.0f / vars::aimbot::speed;
        int move_x = static_cast<int>(current_mouse_pos.x + (tx - current_mouse_pos.x) * smooth_factor);
        int move_y = static_cast<int>(current_mouse_pos.y + (ty - current_mouse_pos.y) * smooth_factor);
        if (move_x == current_mouse_pos.x && tx != current_mouse_pos.x)
            move_x += (tx > current_mouse_pos.x ? 1 : -1);
        if (move_y == current_mouse_pos.y && ty != current_mouse_pos.y)
            move_y += (ty > current_mouse_pos.y ? 1 : -1);
        SetCursorPos(move_x, move_y);
        this->leftover_x = 0.0f;
        this->leftover_y = 0.0f;
    }
    else
    {
        aim_delta_x += this->leftover_x;
        aim_delta_y += this->leftover_y;
        LONG move_x = static_cast<LONG>(aim_delta_x);
        LONG move_y = static_cast<LONG>(aim_delta_y);
        this->leftover_x = aim_delta_x - move_x;
        this->leftover_y = aim_delta_y - move_y;
        INPUT input = {};
        input.type = INPUT_MOUSE;
        input.mi.dx = move_x;
        input.mi.dy = move_y;
        input.mi.dwFlags = MOUSEEVENTF_MOVE;
        SendInput(1, &input, sizeof(INPUT));
    }
}

float c_esp::apply_easing(float t, int style)
{
    t = fmaxf(0.0f, fminf(1.0f, t));

    switch (style)
    {
    case 0: return t;
    case 1: return t;
    case 2: return t * t;
    case 3: return t * (2.0f - t);
    case 4: return (t < 0.5f) ? (2.0f * t * t) : (1.0f - powf(-2.0f * t + 2.0f, 2.0f) / 2.0f);
    default: return t;
    }
}

bool c_esp::is_visible(const vector& from, const vector& to, uintptr_t target_model)
{
    return is_visible(from, to, to, to, to, to, target_model);
}

void c_esp::run_triggerbot(uintptr_t model, uintptr_t p_player_root, float delta_x, float delta_y, vector cam_pos, vector w_target_bone_pos)
{
    static bool trigger_mouse_down = false;
    static std::chrono::steady_clock::time_point trigger_release_time;
    static std::chrono::steady_clock::time_point last_fire_time;
    static bool trigger_waiting = false;
    static std::chrono::steady_clock::time_point trigger_fire_time;

    auto now = std::chrono::steady_clock::now();

    if (trigger_mouse_down)
    {
        if (now >= trigger_release_time)
        {
            mouse_event(MOUSEEVENTF_LEFTUP, 0, 0, 0, 0);
            trigger_mouse_down = false;
        }
        return;
    }

    float distance = sqrtf(delta_x * delta_x + delta_y * delta_y);
    if (distance > vars::triggerbot::fov)
    {
        trigger_waiting = false;
        return;
    }

    auto humanoid = core.find_first_child_class(model, "Humanoid");
    if (!humanoid)
    {
        trigger_waiting = false;
        return;
    }

    float health = memory->read<float>(humanoid + offsets::Health);
    if (health <= 0.0f)
    {
        trigger_waiting = false;
        return;
    }

    uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
    uintptr_t camera = core.find_first_child_class(workspace, "Camera");
    if (camera)
    {
        vector w_player_root = memory->read<vector>(p_player_root + offsets::Position);
        vector w_head = w_target_bone_pos;
        vector w_torso = w_player_root;
        vector w_pelvis = w_player_root;
        vector w_left_foot = w_player_root;
        vector w_right_foot = w_player_root;

        auto torso_part = core.find_first_child(model, "Torso");
        if (!torso_part) torso_part = core.find_first_child(model, "UpperTorso");
        if (torso_part)
        {
            auto p_torso = memory->read<uintptr_t>(torso_part + offsets::Primitive);
            if (p_torso) w_torso = memory->read<vector>(p_torso + offsets::Position);
        }

        auto left_leg = core.find_first_child(model, "Left Leg");
        if (!left_leg) left_leg = core.find_first_child(model, "LeftFoot");
        if (left_leg)
        {
            auto p_left = memory->read<uintptr_t>(left_leg + offsets::Primitive);
            if (p_left) w_left_foot = memory->read<vector>(p_left + offsets::Position);
        }

        auto right_leg = core.find_first_child(model, "Right Leg");
        if (!right_leg) right_leg = core.find_first_child(model, "RightFoot");
        if (right_leg)
        {
            auto p_right = memory->read<uintptr_t>(right_leg + offsets::Primitive);
            if (p_right) w_right_foot = memory->read<vector>(p_right + offsets::Position);
        }

        bool visible = is_visible(cam_pos, w_head, w_torso, w_pelvis, w_left_foot, w_right_foot, model);
        if (!visible)
        {
            trigger_waiting = false;
            return;
        }
    }

    if (!trigger_waiting)
    {
        trigger_fire_time = now + std::chrono::milliseconds(vars::triggerbot::delay);
        trigger_waiting = true;
        return;
    }

    if (now < trigger_fire_time) return;

    if (vars::triggerbot::hit_chance_enabled)
    {
        if ((rand() % 100) > vars::triggerbot::hit_chance)
        {
            trigger_waiting = false;
            return;
        }
    }

    auto time_since_last = std::chrono::duration_cast<std::chrono::milliseconds>(now - last_fire_time).count();
    if (time_since_last < 50) return;

    mouse_event(MOUSEEVENTF_LEFTDOWN, 0, 0, 0, 0);
    trigger_mouse_down = true;
    trigger_release_time = now + std::chrono::milliseconds(vars::triggerbot::hold_time);
    last_fire_time = now;
    trigger_waiting = false;
}

void c_esp::reset_aim_state()
{
    this->leftover_x = 0.0f;
    this->leftover_y = 0.0f;
    this->smoothed_delta_x = 0.0f;
    this->smoothed_delta_y = 0.0f;
}

void c_esp::draw_hitbox_esp(view_matrix_t viewmatrix)
{
    if (!vars::combat::hitbox_expander || !vars::combat::hitbox_visible)
        return;

    if (!g_main::datamodel || !g_main::localplayer)
        return;

    std::vector<uintptr_t> players = get_cached_players(g_main::datamodel);

    for (uintptr_t player : players)
    {
        if (!player || player == g_main::localplayer)
            continue;

        if (vars::combat::hitbox_skip_teammates)
        {
            uintptr_t player_team = memory->read<uintptr_t>(player + offsets::Player::Team);
            if (player_team != 0 && player_team == g_main::localplayer_team)
                continue;
        }

        uintptr_t character = core.get_model_instance(player);
        if (!character) continue;

        uintptr_t hrp = core.find_first_child(character, "HumanoidRootPart");
        if (!hrp) continue;

        uintptr_t primitive = memory->read<uintptr_t>(hrp + offsets::Primitive);
        if (!primitive) continue;

        vector hrp_pos = memory->read<vector>(primitive + offsets::Position);

        float size_x = vars::combat::hitbox_size_x;
        float size_y = vars::combat::hitbox_size_y;
        float size_z = vars::combat::hitbox_size_z;

        vector corners[8] = {
            { hrp_pos.x - size_x / 2, hrp_pos.y - size_y / 2, hrp_pos.z - size_z / 2 },
            { hrp_pos.x + size_x / 2, hrp_pos.y - size_y / 2, hrp_pos.z - size_z / 2 },
            { hrp_pos.x + size_x / 2, hrp_pos.y + size_y / 2, hrp_pos.z - size_z / 2 },
            { hrp_pos.x - size_x / 2, hrp_pos.y + size_y / 2, hrp_pos.z - size_z / 2 },
            { hrp_pos.x - size_x / 2, hrp_pos.y - size_y / 2, hrp_pos.z + size_z / 2 },
            { hrp_pos.x + size_x / 2, hrp_pos.y - size_y / 2, hrp_pos.z + size_z / 2 },
            { hrp_pos.x + size_x / 2, hrp_pos.y + size_y / 2, hrp_pos.z + size_z / 2 },
            { hrp_pos.x - size_x / 2, hrp_pos.y + size_y / 2, hrp_pos.z + size_z / 2 }
        };

        vector2d screen_corners[8];
        bool all_visible = true;

        for (int i = 0; i < 8; i++)
        {
            if (!core.world_to_screen(corners[i], screen_corners[i], viewmatrix))
            {
                all_visible = false;
                break;
            }
        }

        if (!all_visible) continue;

        ImColor hitbox_color = ImColor(255, 0, 0, 150);

        draw.line(ImVec2(screen_corners[0].x, screen_corners[0].y), ImVec2(screen_corners[1].x, screen_corners[1].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[1].x, screen_corners[1].y), ImVec2(screen_corners[2].x, screen_corners[2].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[2].x, screen_corners[2].y), ImVec2(screen_corners[3].x, screen_corners[3].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[3].x, screen_corners[3].y), ImVec2(screen_corners[0].x, screen_corners[0].y), hitbox_color, 1.0f);

        draw.line(ImVec2(screen_corners[4].x, screen_corners[4].y), ImVec2(screen_corners[5].x, screen_corners[5].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[5].x, screen_corners[5].y), ImVec2(screen_corners[6].x, screen_corners[6].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[6].x, screen_corners[6].y), ImVec2(screen_corners[7].x, screen_corners[7].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[7].x, screen_corners[7].y), ImVec2(screen_corners[4].x, screen_corners[4].y), hitbox_color, 1.0f);

        draw.line(ImVec2(screen_corners[0].x, screen_corners[0].y), ImVec2(screen_corners[4].x, screen_corners[4].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[1].x, screen_corners[1].y), ImVec2(screen_corners[5].x, screen_corners[5].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[2].x, screen_corners[2].y), ImVec2(screen_corners[6].x, screen_corners[6].y), hitbox_color, 1.0f);
        draw.line(ImVec2(screen_corners[3].x, screen_corners[3].y), ImVec2(screen_corners[7].x, screen_corners[7].y), hitbox_color, 1.0f);
    }
}

void c_esp::start_hitbox_thread()
{
    if (!c_esp::hitbox_thread_running.exchange(true))
    {
        std::thread([]()
            {
                while (!shutdown_requested)  // FIX: Check shutdown flag
                {
                    if (!vars::combat::hitbox_expander)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        continue;
                    }

                    if (!is_valid_pointer(g_main::datamodel) || !is_valid_pointer(g_main::localplayer))
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(200));
                        continue;
                    }

                    try
                    {
                        std::vector<uintptr_t> players = get_cached_players(g_main::datamodel);

                        if (players.empty())
                        {
                            std::this_thread::sleep_for(std::chrono::milliseconds(100));
                            continue;
                        }

                        c_esp::hitbox_processed_count = 0;

                        vector newSize = {
                            vars::combat::hitbox_size_x,
                            vars::combat::hitbox_size_y,
                            vars::combat::hitbox_size_z
                        };

                        if (newSize.x <= 0.0f || newSize.x > 1000.0f) continue;
                        if (newSize.y <= 0.0f || newSize.y > 1000.0f) continue;
                        if (newSize.z <= 0.0f || newSize.z > 1000.0f) continue;

                        for (uintptr_t player : players)
                        {
                            if (shutdown_requested) break;  // FIX: Check in loop

                            if (!is_valid_pointer(player)) continue;
                            if (player == g_main::localplayer) continue;

                            if (vars::combat::hitbox_skip_teammates)
                            {
                                try
                                {
                                    uintptr_t player_team = memory->read<uintptr_t>(player + offsets::Player::Team);
                                    if (player_team != 0 && player_team == g_main::localplayer_team)
                                        continue;
                                }
                                catch (...)
                                {
                                    continue;
                                }
                            }

                            uintptr_t character = 0;
                            try
                            {
                                character = core.get_model_instance(player);
                            }
                            catch (...)
                            {
                                continue;
                            }

                            if (!is_valid_pointer(character)) continue;

                            uintptr_t hrp = 0;
                            try
                            {
                                hrp = core.find_first_child(character, "HumanoidRootPart");
                            }
                            catch (...)
                            {
                                continue;
                            }

                            if (!is_valid_pointer(hrp)) continue;

                            uintptr_t primitive = 0;
                            try
                            {
                                primitive = memory->read<uintptr_t>(hrp + offsets::Primitive);
                            }
                            catch (...)
                            {
                                continue;
                            }

                            if (!is_valid_pointer(primitive)) continue;

                            try
                            {
                                vector currentSize = memory->read<vector>(primitive + offsets::PartSize);

                                if (currentSize.x <= 0.0f || currentSize.x > 1000.0f) continue;
                                if (currentSize.y <= 0.0f || currentSize.y > 1000.0f) continue;
                                if (currentSize.z <= 0.0f || currentSize.z > 1000.0f) continue;
                            }
                            catch (...)
                            {
                                continue;
                            }

                            try
                            {
                                memory->write<vector>(primitive + offsets::PartSize, newSize);
                                c_esp::hitbox_processed_count++;
                            }
                            catch (...)
                            {
                                continue;
                            }

                            std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        }
                    }
                    catch (...)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
                        continue;
                    }

                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                }

                hitbox_thread_running = false;  // FIX: Mark as not running when exiting
            }).detach();
    }
}

// ==================== BSS FUNCTIONS (unchanged) ====================

void c_esp::server_hop()
{
    util.m_print("[Server Hop] Starting...");

    g_safe_mode = true;

    vars::bss::is_hopping = true;
    vars::bss::is_floating = false;
    vars::bss::going_to_hive = false;
    vars::bss::hive_claimed = false;

    g_main::datamodel = 0;
    g_main::localplayer = 0;
    g_main::v_engine = 0;

    tp_handler.request_rescan();

    std::this_thread::sleep_for(std::chrono::milliseconds(500));

    util.m_print("[Server Hop] Killing Roblox...");
    system("taskkill /F /IM RobloxPlayerBeta.exe >nul 2>&1");

    std::thread([]()
        {
            float wait_time = vars::bss::post_hop_delay;
            util.m_print("[Server Hop] Waiting %.0f seconds for BSS to save...", wait_time);

            for (int i = (int)wait_time; i > 0; i--)
            {
                if (shutdown_requested) return;  // FIX: Check shutdown
                util.m_print("[Server Hop] Opening in %d...", i);
                std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            }

            if (!shutdown_requested)
            {
                util.m_print("[Server Hop] Opening new game...");
                system("start roblox://placeId=1537690962");
            }

        }).detach();
}

void c_esp::run_vicious_esp(view_matrix_t viewmatrix)
{
    if (!g_main::datamodel || !g_main::localplayer) return;
    if (!vars::bss::vicious_esp) return;
    if (vars::bss::is_hopping) return;
    if (!tp_handler.is_ready()) return;

    uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
    if (!workspace) return;

    uintptr_t monsters = core.find_first_child(workspace, "Monsters");
    if (!monsters) return;

    vector local_pos = { 0, 0, 0 };
    uintptr_t local_character = core.find_first_child(workspace, core.get_instance_name(g_main::localplayer));
    if (local_character)
    {
        uintptr_t local_root = core.find_first_child(local_character, "HumanoidRootPart");
        if (local_root)
        {
            uintptr_t p_local_root = memory->read<uintptr_t>(local_root + offsets::Primitive);
            if (p_local_root)
                local_pos = memory->read<vector>(p_local_root + offsets::Position);
        }
    }

    std::vector<uintptr_t> monster_children = core.children(monsters);

    for (uintptr_t monster : monster_children)
    {
        if (!monster) continue;

        std::string name = core.get_instance_name(monster);

        if (name.find("Vicious") == std::string::npos) continue;

        uintptr_t part = core.find_first_child(monster, "HumanoidRootPart");
        if (!part) part = core.find_first_child(monster, "Torso");
        if (!part) part = core.find_first_child(monster, "Head");

        if (!part)
        {
            std::vector<uintptr_t> monster_parts = core.children(monster);
            for (uintptr_t child : monster_parts)
            {
                std::string class_name = core.get_instance_classname(child);
                if (class_name.find("Part") != std::string::npos)
                {
                    part = child;
                    break;
                }
            }
        }

        if (!part) continue;

        uintptr_t primitive = memory->read<uintptr_t>(part + offsets::Primitive);
        if (!primitive) continue;

        vector world_pos = memory->read<vector>(primitive + offsets::Position);
        vector2d screen_pos;

        if (!core.world_to_screen(world_pos, screen_pos, viewmatrix)) continue;

        float distance = sqrtf(
            powf(world_pos.x - local_pos.x, 2) +
            powf(world_pos.y - local_pos.y, 2) +
            powf(world_pos.z - local_pos.z, 2)
        );

        // FIX: Use snprintf instead of stringstream
        snprintf(s_string_buffer, sizeof(s_string_buffer), "VICIOUS BEE [%.0fm]", distance);

        draw.outlined_string(
            ImVec2(screen_pos.x, screen_pos.y),
            s_string_buffer,
            ImColor(255, 0, 0, 255),
            ImColor(0, 0, 0, 255),
            true
        );

        int screen_width = core.get_screen_width();
        int screen_height = core.get_screen_height();

        draw.line(
            ImVec2(screen_width * 0.5f, (float)screen_height),
            ImVec2(screen_pos.x, screen_pos.y),
            ImColor(255, 0, 0, 255),
            2.0f
        );
    }
}

// ... Keep all the remaining BSS functions exactly as they were ...
// (run_vicious_hunter, float_to_target, find_hive_position, test_hive_claiming,
//  track_vicious_status, stay_on_vicious, check_vicious_death, get_session_time,
//  is_server_visited, mark_server_visited, cleanup_old_servers, get_active_blacklist_count,
//  has_friends_in_server, get_current_job_id)

// I'll include just a couple with the fixes applied:

void c_esp::cleanup_old_servers()
{
    static std::chrono::steady_clock::time_point last_cleanup;
    auto now = std::chrono::steady_clock::now();

    if (std::chrono::duration<float>(now - last_cleanup).count() < 30.0f)
        return;

    last_cleanup = now;

    // FIX: Add max size limit
    constexpr size_t MAX_VISITED_SERVERS = 100;

    int removed = 0;
    auto it = vars::bss::visited_servers.begin();
    while (it != vars::bss::visited_servers.end())
    {
        float elapsed = std::chrono::duration<float>(now - it->visit_time).count();

        if (elapsed > vars::bss::server_blacklist_time ||
            vars::bss::visited_servers.size() > MAX_VISITED_SERVERS)
        {
            it = vars::bss::visited_servers.erase(it);
            removed++;
        }
        else
        {
            ++it;
        }
    }

    if (removed > 0)
    {
        util.m_print("[Server] Cleaned up %d old servers from blacklist", removed);
    }
}

float c_esp::get_session_time()
{
    static auto session_start = std::chrono::steady_clock::now();
    static bool initialized = false;

    if (!initialized)
    {
        session_start = std::chrono::steady_clock::now();
        initialized = true;
    }

    auto now = std::chrono::steady_clock::now();
    return std::chrono::duration<float>(now - session_start).count();
}

// ... Include the rest of your BSS functions unchanged ...

void c_esp::run_vicious_hunter()
{
    if (vars::bss::is_hopping) return;
    if (!vars::bss::vicious_hunter) return;
    if (vars::bss::vicious_found) return;
    if (!tp_handler.is_ready()) return;
    if (!g_main::datamodel || !g_main::localplayer) return;

    static auto join_time = std::chrono::steady_clock::now();
    static bool timer_started = false;
    static bool just_hopped = false;
    static bool server_checked = false;

    if (!timer_started)
    {
        join_time = std::chrono::steady_clock::now();
        timer_started = true;
        server_checked = false;
        just_hopped = (vars::bss::servers_checked > 0);
        if (just_hopped) util.m_print("[Vicious Hunter] Waiting for BSS to load...");
        return;
    }

    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float>(now - join_time).count();
    float required_delay = just_hopped ? vars::bss::server_load_delay : vars::bss::check_delay;

    if (elapsed < required_delay) return;

    // Mark this server as visited (only once per server)
    if (!server_checked)
    {
        server_checked = true;

        // Check if we already visited this server
        std::string job_id = get_current_job_id();
        if (is_server_visited(job_id))
        {
            util.m_print("[Vicious Hunter] Already visited this server, hopping...");
            vars::bss::servers_checked++;
            timer_started = false;
            server_hop();
            return;
        }

        // Check if friends are in server
        if (has_friends_in_server())
        {
            util.m_print("[Vicious Hunter] Friends in server, skipping...");
            timer_started = false;
            server_hop();
            return;
        }

        // Mark as visited
        mark_server_visited();
    }

    just_hopped = false;
    timer_started = false;

    uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
    if (!workspace) return;

    uintptr_t monsters = core.find_first_child(workspace, "Monsters");
    if (!monsters) return;

    bool found = false;
    uintptr_t vicious_monster = 0;

    std::vector<uintptr_t> monster_children = core.children(monsters);

    for (uintptr_t monster : monster_children)
    {
        if (!monster) continue;
        std::string name = core.get_instance_name(monster);
        if (name.find("Vicious") != std::string::npos)
        {
            found = true;
            vicious_monster = monster;
            break;
        }
    }

    if (found)
    {
        vars::bss::vicious_found = true;
        vars::bss::is_hopping = false;
        MessageBeep(MB_ICONEXCLAMATION);
        util.m_print("[Vicious Hunter] FOUND! Servers checked: %d", vars::bss::servers_checked);

        if (vars::bss::webhook_enabled && !vars::bss::webhook_url.empty())
        {
            std::string webhook_cmd = "start /B curl -X POST -H \"Content-Type: application/json\" -d \"{\\\"content\\\":\\\"**Vicious Bee Found!** Servers checked: " + std::to_string(vars::bss::servers_checked) + "\\\"}\" \"" + vars::bss::webhook_url + "\" >nul 2>&1";
            system(webhook_cmd.c_str());
        }

        float vicious_x = 0, vicious_y = 0, vicious_z = 0;
        bool has_vicious_pos = false;

        if (vars::bss::float_to_vicious && vicious_monster)
        {
            uintptr_t vicious_part = core.find_first_child(vicious_monster, "HumanoidRootPart");
            if (!vicious_part) vicious_part = core.find_first_child(vicious_monster, "Torso");
            if (!vicious_part) vicious_part = core.find_first_child(vicious_monster, "Head");

            if (!vicious_part)
            {
                std::vector<uintptr_t> parts = core.children(vicious_monster);
                for (uintptr_t p : parts)
                {
                    std::string class_name = core.get_instance_classname(p);
                    if (class_name.find("Part") != std::string::npos)
                    {
                        vicious_part = p;
                        break;
                    }
                }
            }

            if (vicious_part)
            {
                uintptr_t prim = memory->read<uintptr_t>(vicious_part + offsets::Primitive);
                if (prim)
                {
                    vector vpos = memory->read<vector>(prim + offsets::Position);
                    vicious_x = vpos.x;
                    vicious_y = vpos.y + 50.0f;
                    vicious_z = vpos.z;
                    has_vicious_pos = true;
                }
            }

            if (vars::bss::need_hive_first && !vars::bss::hive_claimed)
            {
                float hive_x, hive_y, hive_z;
                if (find_hive_position(hive_x, hive_y, hive_z))
                {
                    util.m_print("[Vicious Hunter] Going to hive first...");
                    vars::bss::target_x = hive_x;
                    vars::bss::target_y = hive_y;
                    vars::bss::target_z = hive_z;
                    vars::bss::going_to_hive = true;
                    vars::bss::is_floating = true;
                    vars::bss::stored_vicious_x = vicious_x;
                    vars::bss::stored_vicious_y = vicious_y;
                    vars::bss::stored_vicious_z = vicious_z;

                    // Debug to confirm flags are set
                    util.m_print("[Vicious Hunter] FLAGS SET: is_floating=%d, going_to_hive=%d",
                        vars::bss::is_floating, vars::bss::going_to_hive);
                }
                else
                {
                    util.m_print("[Vicious Hunter] No hive found, going to Vicious...");
                    if (has_vicious_pos) {
                        vars::bss::target_x = vicious_x;
                        vars::bss::target_y = vicious_y;
                        vars::bss::target_z = vicious_z;
                        vars::bss::is_floating = true;
                    }
                }
            }
            else
            {
                if (has_vicious_pos) {
                    util.m_print("[Vicious Hunter] Going to Vicious (High Altitude)...");
                    vars::bss::target_x = vicious_x;
                    vars::bss::target_y = vicious_y;
                    vars::bss::target_z = vicious_z;
                    vars::bss::is_floating = true;
                }
            }
        }
    }
    else
    {
        vars::bss::servers_checked++;
        vars::bss::is_hopping = true;
        timer_started = false;
        util.m_print("[Vicious Hunter] Not found. Hopping... (Server #%d)", vars::bss::servers_checked);
        server_hop();
    }
}

void c_esp::float_to_target()
{
    // 1. Basic checks
    if (vars::bss::is_hopping) {
        vars::bss::is_floating = false;
        return;
    }
    if (!vars::bss::is_floating) return;
    if (!g_main::datamodel || !g_main::localplayer) return;
    if (!tp_handler.is_ready()) return;

    // 2. State variables
    static bool is_claiming = false;
    static std::chrono::steady_clock::time_point claim_start_time;
    static int press_count = 0;
    static std::chrono::steady_clock::time_point last_press_time;
    static HWND roblox_hwnd = NULL;

    // Cache variables
    static uintptr_t cached_workspace = 0;
    static uintptr_t cached_character = 0;
    static uintptr_t cached_hrp = 0;
    static uintptr_t cached_primitive = 0;
    static std::chrono::steady_clock::time_point last_cache_update;

    auto now = std::chrono::steady_clock::now();
    float ms_since_cache = std::chrono::duration<float, std::milli>(now - last_cache_update).count();

    // 3. Update Cache (500ms)
    if (ms_since_cache > 500.0f || cached_primitive == 0)
    {
        last_cache_update = now;
        cached_workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
        if (!cached_workspace) return;
        cached_character = core.find_first_child(cached_workspace, core.get_instance_name(g_main::localplayer));
        if (!cached_character) return;
        cached_hrp = core.find_first_child(cached_character, "HumanoidRootPart");
        if (!cached_hrp) return;
        cached_primitive = memory->read<uintptr_t>(cached_hrp + offsets::Primitive);
        if (!cached_primitive) return;
    }

    if (!cached_primitive) return;

    // 4. Calculate Distance
    vector current_pos = memory->read<vector>(cached_primitive + offsets::Position);
    float dx = vars::bss::target_x - current_pos.x;
    float dy = vars::bss::target_y - current_pos.y;
    float dz = vars::bss::target_z - current_pos.z;
    float distance = sqrtf(dx * dx + dy * dy + dz * dz);

    // ==========================================
    // CLAIMING LOGIC (If at hive)
    // ==========================================
    if (is_claiming && vars::bss::going_to_hive)
    {
        float elapsed = std::chrono::duration<float>(now - claim_start_time).count();

        // Stop movement while claiming
        memory->write<vector>(cached_primitive + offsets::Velocity, vector{ 0, 0, 0 });

        // Keep position locked at hive
        vector hive_pos;
        hive_pos.x = vars::bss::target_x;
        hive_pos.y = vars::bss::target_y;
        hive_pos.z = vars::bss::target_z;
        memory->write<vector>(cached_primitive + offsets::Position, hive_pos);

        // Press E (every 150ms)
        float ms_since_last_press = std::chrono::duration<float, std::milli>(now - last_press_time).count();
        if (ms_since_last_press > 150.0f)
        {
            last_press_time = now;
            press_count++;

            INPUT inputs[2] = {};
            inputs[0].type = INPUT_KEYBOARD;
            inputs[0].ki.wScan = 0x12; // E
            inputs[0].ki.dwFlags = KEYEVENTF_SCANCODE;
            inputs[1].type = INPUT_KEYBOARD;
            inputs[1].ki.wScan = 0x12; // E
            inputs[1].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            SendInput(1, &inputs[0], sizeof(INPUT));
            SendInput(1, &inputs[1], sizeof(INPUT));

            if (roblox_hwnd) {
                PostMessageA(roblox_hwnd, WM_KEYDOWN, 'E', 0);
                PostMessageA(roblox_hwnd, WM_KEYUP, 'E', 0);
            }
        }

        // Check if claimed (every 1 second)
        static std::chrono::steady_clock::time_point last_claim_check;
        float ms_since_check = std::chrono::duration<float, std::milli>(now - last_claim_check).count();

        if (ms_since_check > 1000.0f)
        {
            last_claim_check = now;

            uintptr_t hive_platforms = core.find_first_child(cached_workspace, "HivePlatforms");
            if (hive_platforms)
            {
                std::vector<uintptr_t> platforms = core.children(hive_platforms);
                for (uintptr_t platform : platforms)
                {
                    if (!platform) continue;
                    std::string name = core.get_instance_name(platform);
                    if (name != "Platform") continue;

                    uintptr_t player_ref = core.find_first_child(platform, "PlayerRef");
                    if (!player_ref) continue;

                    uintptr_t owner = memory->read<uintptr_t>(player_ref + offsets::Misc::Value);

                    if (owner == g_main::localplayer)
                    {
                        util.m_print("[Hive] SUCCESS! Hive claimed.");

                        // === TRANSITION TO VICIOUS ===
                        is_claiming = false;
                        vars::bss::hive_claimed = true;
                        vars::bss::going_to_hive = false; // Turn OFF hive mode

                        // Set new target
                        vars::bss::target_x = vars::bss::stored_vicious_x;
                        vars::bss::target_y = vars::bss::stored_vicious_y;
                        vars::bss::target_z = vars::bss::stored_vicious_z;

                        util.m_print("[Hive] Switching target to Vicious...");
                        return; // Return now, next frame will use Velocity to go to new target
                    }
                }
            }
        }

        // Timeout
        if (elapsed > 15.0f)
        {
            util.m_print("[Hive] Timeout! Going to Vicious anyway.");
            is_claiming = false;
            vars::bss::hive_claimed = true;
            vars::bss::going_to_hive = false;

            vars::bss::target_x = vars::bss::stored_vicious_x;
            vars::bss::target_y = vars::bss::stored_vicious_y;
            vars::bss::target_z = vars::bss::stored_vicious_z;
        }
        return;
    }

    // ==========================================
    // ARRIVAL LOGIC
    // ==========================================
    if (distance < 10.0f)
    {
        memory->write<vector>(cached_primitive + offsets::Velocity, vector{ 0, 0, 0 });

        // If arrived at hive -> Start claiming
        if (vars::bss::going_to_hive && !is_claiming)
        {
            util.m_print("[Hive] Arrived! Starting claim...");
            roblox_hwnd = FindWindowA(NULL, "Roblox");
            if (!roblox_hwnd) roblox_hwnd = FindWindowA("WINDOWSCLIENT", NULL);
            if (roblox_hwnd) SetForegroundWindow(roblox_hwnd);

            is_claiming = true;
            claim_start_time = now;
            last_press_time = now;
            press_count = 0;
            return;
        }

        // If arrived at Vicious (going_to_hive is false)
        if (!vars::bss::going_to_hive)
        {
            vars::bss::is_floating = false;
            util.m_print("[Float] Arrived at Vicious!");
        }
        return;
    }

    // ==========================================
    // VELOCITY MOVEMENT (Your preferred method)
    // ==========================================

    // Noclip occasionally
    static std::chrono::steady_clock::time_point last_noclip_time;
    if (std::chrono::duration<float, std::milli>(now - last_noclip_time).count() > 1000.0f)
    {
        last_noclip_time = now;
        std::vector<uintptr_t> children = core.children(cached_character);
        for (auto child : children) {
            std::string className = core.get_instance_classname(child);
            if (className.find("Part") != std::string::npos || className.find("Mesh") != std::string::npos) {
                uintptr_t prim = memory->read<uintptr_t>(child + offsets::Primitive);
                if (prim) memory->write<bool>(prim + offsets::CanCollide, false);
            }
        }
    }

    float nx = dx / distance;
    float ny = dy / distance;
    float nz = dz / distance;

    vector velocity;
    velocity.x = nx * vars::bss::float_speed;
    velocity.y = ny * vars::bss::float_speed;
    velocity.z = nz * vars::bss::float_speed;

    memory->write<vector>(cached_primitive + offsets::Velocity, velocity);
}


bool c_esp::find_hive_position(float& out_x, float& out_y, float& out_z)
{
    if (!g_main::datamodel || !g_main::localplayer) return false;

    uintptr_t workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
    if (!workspace) return false;

    uintptr_t hive_platforms = core.find_first_child(workspace, "HivePlatforms");
    if (!hive_platforms) {
        util.m_print("[Hive] HivePlatforms folder not found");
        return false;
    }

    std::vector<uintptr_t> platforms = core.children(hive_platforms);

    uintptr_t my_platform = 0;
    uintptr_t unclaimed_platform = 0;

    int claimed_count = 0;
    int unclaimed_count = 0;

    util.m_print("[Hive] ===== SCANNING ALL HIVES =====");

    for (uintptr_t platform : platforms)
    {
        if (!platform) continue;

        std::string name = core.get_instance_name(platform);
        if (name != "Platform") continue;

        // Get the Hive ObjectValue to find actual hive number
        std::string hive_name = "Unknown";
        uintptr_t hive_obj = core.find_first_child(platform, "Hive");
        if (hive_obj)
        {
            uintptr_t hive_ref = memory->read<uintptr_t>(hive_obj + offsets::Misc::Value);
            if (hive_ref)
            {
                hive_name = core.get_instance_name(hive_ref);
            }
        }

        // Get platform position for reference
        uintptr_t circle = core.find_first_child(platform, "Circle");
        float pos_x = 0, pos_z = 0;
        if (circle)
        {
            uintptr_t prim = memory->read<uintptr_t>(circle + offsets::Primitive);
            if (prim)
            {
                vector pos = memory->read<vector>(prim + offsets::Position);
                pos_x = pos.x;
                pos_z = pos.z;
            }
        }

        // Check PlayerRef ObjectValue to see who owns this hive
        uintptr_t player_ref = core.find_first_child(platform, "PlayerRef");
        if (!player_ref) continue;

        uintptr_t owner = memory->read<uintptr_t>(player_ref + offsets::Misc::Value);

        if (owner == 0)
        {
            unclaimed_count++;
            util.m_print("[Hive] %s - UNCLAIMED (pos: %.0f, %.0f)", hive_name.c_str(), pos_x, pos_z);

            if (unclaimed_platform == 0)
            {
                unclaimed_platform = platform;
            }
        }
        else if (owner == g_main::localplayer)
        {
            claimed_count++;
            util.m_print("[Hive] %s - YOURS! (pos: %.0f, %.0f)", hive_name.c_str(), pos_x, pos_z);
            my_platform = platform;
            vars::bss::hive_claimed = true;
        }
        else
        {
            claimed_count++;
            std::string owner_name = core.get_instance_name(owner);
            if (!owner_name.empty())
            {
                util.m_print("[Hive] %s - CLAIMED by '%s'", hive_name.c_str(), owner_name.c_str());
            }
            else
            {
                util.m_print("[Hive] %s - CLAIMED (unknown)", hive_name.c_str());
            }
        }
    }

    util.m_print("[Hive] ===== SCAN COMPLETE =====");
    util.m_print("[Hive] Claimed: %d | Unclaimed: %d", claimed_count, unclaimed_count);

    // Prefer our hive, otherwise use unclaimed
    uintptr_t target_platform = my_platform ? my_platform : unclaimed_platform;

    if (!target_platform)
    {
        util.m_print("[Hive] No suitable hive found!");
        return false;
    }

    if (my_platform)
    {
        util.m_print("[Hive] Going to YOUR hive");
    }
    else if (unclaimed_platform)
    {
        util.m_print("[Hive] Going to unclaimed hive");
    }

    // Get position from Circle part
    uintptr_t circle = core.find_first_child(target_platform, "Circle");
    if (!circle)
    {
        std::vector<uintptr_t> children = core.children(target_platform);
        for (uintptr_t child : children)
        {
            std::string class_name = core.get_instance_classname(child);
            if (class_name.find("Part") != std::string::npos)
            {
                circle = child;
                break;
            }
        }
    }

    if (!circle)
    {
        util.m_print("[Hive] No part found in platform");
        return false;
    }

    uintptr_t primitive = memory->read<uintptr_t>(circle + offsets::Primitive);
    if (!primitive)
    {
        util.m_print("[Hive] No primitive found");
        return false;
    }

    vector pos = memory->read<vector>(primitive + offsets::Position);
    out_x = pos.x;
    out_y = pos.y + 5.0f;
    out_z = pos.z;

    util.m_print("[Hive] Target Position: %.1f, %.1f, %.1f", out_x, out_y, out_z);
    return true;
}

void c_esp::test_hive_claiming()
{
    // Just return if not testing. DO NOT reset is_floating here because Vicious Hunter needs it!
    if (!vars::bss::test_hive_claim) return;

    if (vars::bss::hive_claimed) {
        util.m_print("[Hive Test] Already claimed, disabling test");
        vars::bss::test_hive_claim = false;
        vars::bss::is_floating = false; // Safe to reset here since we are turning off the test
        return;
    }

    if (!vars::bss::is_floating) {
        util.m_print("[Hive Test] Finding hive...");
        if (find_hive_position(vars::bss::target_x, vars::bss::target_y, vars::bss::target_z))
        {
            vars::bss::going_to_hive = true;
            vars::bss::is_floating = true;
        }
    }
}

void c_esp::track_vicious_status()
{
    if (!g_main::datamodel || !g_main::localplayer) return;
    if (vars::bss::is_hopping) return;
    if (!vars::bss::vicious_found) return;

    // DON'T track while going to hive or floating
    if (vars::bss::going_to_hive) return;
    if (vars::bss::is_floating) return;

    static uintptr_t cached_workspace = 0;
    static uintptr_t cached_monsters = 0;
    static std::chrono::steady_clock::time_point last_cache_time;

    auto now = std::chrono::steady_clock::now();
    float ms_since_cache = std::chrono::duration<float, std::milli>(now - last_cache_time).count();

    if (ms_since_cache > 300.0f || cached_workspace == 0)
    {
        last_cache_time = now;
        cached_workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
        if (cached_workspace)
        {
            cached_monsters = core.find_first_child(cached_workspace, "Monsters");
        }
    }

    if (!cached_monsters)
    {
        vars::bss::vicious_alive = false;
        return;
    }

    std::vector<uintptr_t> monsters = core.children(cached_monsters);
    bool found_vicious = false;

    for (uintptr_t monster : monsters)
    {
        if (!monster) continue;

        std::string name = core.get_instance_name(monster);
        if (name.find("Vicious") != std::string::npos)
        {
            found_vicious = true;
            break;
        }
    }

    vars::bss::vicious_alive = found_vicious;
}

void c_esp::stay_on_vicious()
{
    // Basic checks
    if (!vars::bss::vicious_found) return;
    if (!vars::bss::vicious_alive) return;
    if (vars::bss::is_hopping) return;
    if (vars::bss::going_to_hive) return;

    // Don't interfere while floating to target initially
    if (vars::bss::is_floating) return;

    // Wait for hive claim
    if (vars::bss::need_hive_first && !vars::bss::hive_claimed) return;

    if (!g_main::datamodel || !g_main::localplayer) return;

    // Set tracking flag
    vars::bss::tracking_vicious = true;

    // State variables for safe hovering
    static bool has_touched = false;
    static uintptr_t last_vicious_ptr = 0;

    // Cache pointers (500ms)
    static uintptr_t cached_workspace = 0;
    static uintptr_t cached_character = 0;
    static uintptr_t cached_primitive = 0;
    static std::chrono::steady_clock::time_point last_cache_time;

    auto now = std::chrono::steady_clock::now();
    float ms_since_cache = std::chrono::duration<float, std::milli>(now - last_cache_time).count();

    if (ms_since_cache > 500.0f || cached_workspace == 0)
    {
        last_cache_time = now;
        cached_workspace = core.find_first_child_class(g_main::datamodel, "Workspace");
        if (!cached_workspace) return;
        cached_character = core.find_first_child(cached_workspace, core.get_instance_name(g_main::localplayer));
        if (!cached_character) return;
        uintptr_t hrp = core.find_first_child(cached_character, "HumanoidRootPart");
        if (!hrp) return;
        cached_primitive = memory->read<uintptr_t>(hrp + offsets::Primitive);
    }

    if (!cached_primitive || !cached_workspace) return;

    uintptr_t monsters = core.find_first_child(cached_workspace, "Monsters");
    if (!monsters) return;

    std::vector<uintptr_t> monster_list = core.children(monsters);

    for (uintptr_t monster : monster_list)
    {
        if (!monster) continue;

        std::string name = core.get_instance_name(monster);
        if (name.find("Vicious") == std::string::npos) continue;

        // Reset touch state if it's a new Vicious instance
        if (monster != last_vicious_ptr)
        {
            last_vicious_ptr = monster;
            has_touched = false;
            util.m_print("[Vicious] New target found. Resetting touch state.");
        }

        // Get Vicious Position
        uintptr_t vicious_part = core.find_first_child(monster, "HumanoidRootPart");
        if (!vicious_part) vicious_part = core.find_first_child(monster, "Torso");
        if (!vicious_part) vicious_part = core.find_first_child(monster, "Head");
        if (!vicious_part) continue;

        uintptr_t vicious_prim = memory->read<uintptr_t>(vicious_part + offsets::Primitive);
        if (!vicious_prim) continue;

        vector vicious_pos = memory->read<vector>(vicious_prim + offsets::Position);
        vector current_pos = memory->read<vector>(cached_primitive + offsets::Position);

        // Calculate distance
        float dx = vicious_pos.x - current_pos.x;
        float dy = vicious_pos.y - current_pos.y;
        float dz = vicious_pos.z - current_pos.z;
        float distance = sqrtf(dx * dx + dy * dy + dz * dz);

        // Logic: Touch first, then Hover
        vector target_pos = vicious_pos;

        if (!has_touched)
        {
            // GOAL: Go inside the bee to trigger it
            target_pos = vicious_pos;

            if (distance < 8.0f) // Close enough to touch
            {
                has_touched = true;
                util.m_print("[Vicious] Touched! Backing off to safe distance...");
            }
        }
        else
        {
            // GOAL: Hover safely above the bee
            // +25 studs Y is usually safe from spikes
            target_pos.x = vicious_pos.x;
            target_pos.y = vicious_pos.y + 25.0f;
            target_pos.z = vicious_pos.z;
        }

        // Move towards specific target (Touch or Safe Hover)
        float tdx = target_pos.x - current_pos.x;
        float tdy = target_pos.y - current_pos.y;
        float tdz = target_pos.z - current_pos.z;
        float target_dist = sqrtf(tdx * tdx + tdy * tdy + tdz * tdz);

        if (target_dist > 2.0f)
        {
            // Move fast towards safety
            float nx = tdx / target_dist;
            float ny = tdy / target_dist;
            float nz = tdz / target_dist;

            float speed = 60.0f; // Fast enough to dodge, slow enough to track

            vector velocity;
            velocity.x = nx * speed;
            velocity.y = ny * speed;
            velocity.z = nz * speed;

            memory->write<vector>(cached_primitive + offsets::Velocity, velocity);
        }
        else
        {
            // Hold position relative to bee
            memory->write<vector>(cached_primitive + offsets::Velocity, vector{ 0, 0, 0 });
        }

        // Only track one vicious bee
        break;
    }
}
void c_esp::check_vicious_death()
{
    if (!vars::bss::vicious_hunter) return;
    if (!vars::bss::vicious_found) return;
    if (vars::bss::is_hopping) return;
    if (vars::bss::is_floating) return;
    if (vars::bss::going_to_hive) return;

    // Only check death after we're actually attacking
    if (vars::bss::need_hive_first && !vars::bss::hive_claimed) return;

    static bool was_alive = false;
    static bool confirming_death = false;
    static std::chrono::steady_clock::time_point death_start;

    auto now = std::chrono::steady_clock::now();

    if (!was_alive && vars::bss::vicious_alive)
    {
        was_alive = true;
        vars::bss::tracking_vicious = true;
        util.m_print("[Vicious] Started attacking!");
    }

    if (was_alive && !vars::bss::vicious_alive)
    {
        if (!confirming_death)
        {
            confirming_death = true;
            death_start = now;
            util.m_print("[Vicious] Disappeared, confirming kill...");
            return;
        }

        float confirm_time = std::chrono::duration<float>(now - death_start).count();

        if (confirm_time > 3.0f)
        {
            confirming_death = false;
            was_alive = false;
            vars::bss::tracking_vicious = false;
            vars::bss::vicious_kills++;

            util.m_print("[Vicious] ===== KILLED! =====");
            util.m_print("[Vicious] Total Kills: %d", vars::bss::vicious_kills);
            util.m_print("[Vicious] Servers Checked: %d", vars::bss::servers_checked);

            if (vars::bss::webhook_enabled && !vars::bss::webhook_url.empty())
            {
                float session_time = get_session_time();
                int minutes = (int)(session_time / 60);
                int seconds = (int)session_time % 60;

                char msg[512];
                sprintf_s(msg, "**Vicious Bee Killed!**\\nKills: %d\\nServers: %d\\nTime: %dm %ds",
                    vars::bss::vicious_kills,
                    vars::bss::servers_checked,
                    minutes, seconds);

                std::string webhook_cmd = "start /B curl -X POST -H \"Content-Type: application/json\" -d \"{\\\"content\\\":\\\"";
                webhook_cmd += msg;
                webhook_cmd += "\\\"}\" \"" + vars::bss::webhook_url + "\" >nul 2>&1";
                system(webhook_cmd.c_str());
            }

            util.m_print("[Vicious] Hopping to find another...");

            vars::bss::vicious_found = false;
            vars::bss::hive_claimed = false;
            vars::bss::is_floating = false;
            vars::bss::going_to_hive = false;

            server_hop();
        }
    }

    if (confirming_death && vars::bss::vicious_alive)
    {
        confirming_death = false;
        util.m_print("[Vicious] Still alive, continuing attack...");
    }
}

bool c_esp::is_server_visited(const std::string& job_id)
{
    if (job_id.empty()) return false;

    auto now = std::chrono::steady_clock::now();

    // Clean up old entries while checking
    auto it = vars::bss::visited_servers.begin();
    while (it != vars::bss::visited_servers.end())
    {
        float elapsed = std::chrono::duration<float>(now - it->visit_time).count();

        // Remove if older than blacklist time
        if (elapsed > vars::bss::server_blacklist_time)
        {
            util.m_print("[Server] Unblacklisted old server: %s (%.0f seconds old)",
                it->job_id.substr(0, 8).c_str(), elapsed);
            it = vars::bss::visited_servers.erase(it);
        }
        else
        {
            // Check if this is the server we're looking for
            if (it->job_id == job_id)
            {
                float time_left = vars::bss::server_blacklist_time - elapsed;
                util.m_print("[Server] Already visited (%.0f seconds remaining)", time_left);
                return true;
            }
            ++it;
        }
    }

    return false;
}

void c_esp::mark_server_visited()
{
    std::string job_id = get_current_job_id();
    if (job_id.empty()) return;

    // Check if already in list (update timestamp if so)
    for (auto& server : vars::bss::visited_servers)
    {
        if (server.job_id == job_id)
        {
            server.visit_time = std::chrono::steady_clock::now();
            return;
        }
    }

    // Add new server
    vars::bss::ServerEntry entry;
    entry.job_id = job_id;
    entry.visit_time = std::chrono::steady_clock::now();

    vars::bss::visited_servers.push_back(entry);
    vars::bss::current_job_id = job_id;

    util.m_print("[Server] Marked as visited: %s", job_id.substr(0, 8).c_str());
    util.m_print("[Server] Total blacklisted: %d", (int)vars::bss::visited_servers.size());
}

// Add a function to clean up old servers periodically

// Optional: Get active blacklist count
int c_esp::get_active_blacklist_count()
{
    auto now = std::chrono::steady_clock::now();
    int count = 0;

    for (const auto& server : vars::bss::visited_servers)
    {
        float elapsed = std::chrono::duration<float>(now - server.visit_time).count();
        if (elapsed <= vars::bss::server_blacklist_time)
        {
            count++;
        }
    }

    return count;
}

bool c_esp::has_friends_in_server()
{
    if (!vars::bss::avoid_friends) return false;
    if (!g_main::datamodel) return false;
    if (vars::bss::blacklisted_friends.empty()) return false;

    uintptr_t players_service = core.find_first_child_class(g_main::datamodel, "Players");
    if (!players_service) return false;

    std::vector<uintptr_t> players = core.children(players_service);

    for (uintptr_t player : players)
    {
        if (!player) continue;
        if (player == g_main::localplayer) continue;

        std::string player_name = core.get_instance_name(player);

        // Check against blacklisted friends
        for (const auto& friend_name : vars::bss::blacklisted_friends)
        {
            if (player_name == friend_name)
            {
                util.m_print("[Server] Friend '%s' found in server, skipping!", friend_name.c_str());
                return true;
            }
        }
    }

    return false;
}

std::string c_esp::get_current_job_id()
{
    // Use datamodel address as unique server identifier
    if (!g_main::datamodel) return "";

    char buffer[32];
    sprintf_s(buffer, "%llX", g_main::datamodel);
    return std::string(buffer);
}