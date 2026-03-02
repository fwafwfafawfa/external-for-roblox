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