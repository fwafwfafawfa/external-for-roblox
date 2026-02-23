#pragma once

#include "../../../main.hpp"
#include <mutex>
#include <atomic>
#include <unordered_map>

struct WorldPart
{
    vector pos;
    vector size;
    float vol;
    bool large;
};

struct TargetData
{
    vector last_velocity;
    vector last_position;
    std::chrono::steady_clock::time_point last_update;
};

class c_esp
{
public:
    float leftover_x = 0.0f;
    float leftover_y = 0.0f;
    float smoothed_delta_x = 0.0f;
    float smoothed_delta_y = 0.0f;
    uintptr_t locked_target = 0;
    vector last_target_pos = { 0, 0, 0 };

    static std::vector<WorldPart> geometry;
    static double last_refresh;
    static std::atomic<bool> building;
    static std::atomic<bool> ready;
    static std::unordered_map<uintptr_t, std::pair<bool, std::chrono::steady_clock::time_point>> vis_cache;
    static std::mutex geometry_mtx;
    static std::mutex vis_cache_mtx;
    static std::unordered_map<uintptr_t, TargetData> target_tracking;
    static std::mutex tracking_mtx;
    static void hitbox_expander_thread();
    static void start_hitbox_thread();
    static int hitbox_processed_count;
    static void apply_hitbox_expander();
    static std::mutex players_mtx;

    const char* get_target_bone_name(uintptr_t model, uintptr_t player);
    const char* get_closest_part_name(uintptr_t model);
    const char* get_random_part_name();
    vector predict_position(vector current_pos, vector velocity, vector cam_pos);
    float apply_easing(float t, int style);
    void perform_aim(float delta_x, float delta_y, float target_x, float target_y);
    void run_triggerbot(uintptr_t model, uintptr_t p_player_root, float delta_x, float delta_y, vector cam_pos, vector w_target_bone_pos);
    void reset_aim_state();
    void update_world_cache();
    void calculate_fps();
    void run_players(view_matrix_t viewmatrix);
    void run_aimbot(view_matrix_t viewmatrix);
    void draw_hitbox_esp(view_matrix_t viewmatrix);
    void draw_minimap(view_matrix_t viewmatrix);
    void run_vicious_esp(view_matrix_t viewmatrix);
    void run_vicious_hunter();
    void server_hop();
    void load_hunter_state();
    void float_to_target();
    void test_hive_claiming();
    void track_vicious_status();
    void stay_on_vicious();
    void check_vicious_death();
    void display_stats();
    float get_session_time();
    std::string get_current_job_id();
    bool is_server_visited(const std::string& job_id);
    void mark_server_visited();
    bool has_friends_in_server();
    void cleanup_old_servers();  // Add this
    int get_active_blacklist_count();  // And this
    static std::atomic<bool> shutdown_requested;
    static void shutdown();
    static void cleanup_vis_cache();
    void DrawSkeleton(uintptr_t model, view_matrix_t viewmatrix);

    bool is_visible(const vector& from, const vector& to, uintptr_t target_model);
    bool is_visible(const vector& from, const vector& head, const vector& torso, const vector& pelvis, const vector& left_foot, const vector& right_foot, uintptr_t target_model);
    bool find_hive_position(float& out_x, float& out_y, float& out_z);

private:
    static std::atomic<bool> hitbox_thread_running;
};

inline c_esp esp;
