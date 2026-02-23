#pragma once

#include <windows.h>
#include "../addons/imgui/imgui.h" // Corrected Include ImGui for ImColor
#include <cmath> // For sqrtf
#include <string> // Required for std::string
#include <chrono>
#include <vector>

// Define 
// and CFrame structs first

struct Matrix3 {
    float data[9]; // Represents a 3x3 matrix

    Matrix3() {
        for (int i = 0; i < 9; ++i) data[i] = 0.0f;
    }
};

// Now define the vars namespace that uses the vector struct
namespace vars
{
    namespace esp
    {

        inline bool toggled = true;
        inline bool show_health = true;
        inline bool show_distance = true;
        inline bool show_skeleton = false;
        inline bool show_tracers = true;
        inline bool show_box = true;
        inline bool hide_dead = true;
        inline bool hide_teammates = true;
        inline bool show_fps = true;
        inline bool show_weapon = true;
        inline bool chams_enabled = false;
        inline bool chams_visible_only = false;
        inline bool chams_enemies_only = true;
        inline float chams_transparency = 0.5f;
        inline float chams_opacity = 0.5f;
        inline bool show_vicious = true;

        inline ImColor esp_box_color = ImColor(255, 255, 255, 255);
        inline ImColor esp_name_color = ImColor(255, 255, 255, 255);
        inline ImColor esp_distance_color = ImColor(255, 255, 255, 255);
        inline ImColor esp_skeleton_color = ImColor(255, 0, 0, 255);
        inline ImColor esp_tracer_color = ImColor(255, 255, 255, 255);
        inline ImColor chams_visible_color = ImColor(0, 255, 0, 255);
        inline ImColor chams_invisible_color = ImColor(255, 0, 0, 255);
    }

    namespace triggerbot
    {
        inline bool toggled = false;
        inline int activation_key = VK_XBUTTON1;
        inline bool ignore_teammates = true;
        inline float fov = 15.0f;
        inline int delay = 50;
        inline int hold_time = 50;
        inline bool use_prediction = true;      // NEW: Use aimbot's prediction
        inline bool only_when_aiming = false;   // NEW: Only trigger when aimbot is aiming
        inline bool hit_chance_enabled = false; // NEW: Random chance to fire
        inline int hit_chance = 100;            // NEW: Percentage (0-100)
    }

    namespace aimbot
    {
        inline bool toggled = true;
        inline float speed = 1.0f;
        inline float fov = 100.0f;
        inline float deadzone = 5.0f;
        inline bool use_set_cursor_pos = false;
        inline float smoothing_factor = 0.2f;
        inline int activation_key = 0x02;
        inline int aimbot_hitbox = 0; // 0 = Head, 1 = Body ()
        inline bool ignore_teammates = true;
        inline bool show_fov_circle = true; // New: Toggle for FOV circle
        inline bool prediction = false; // New: Toggle for aimbot prediction
        inline float prediction_x = 10.0f;  // Lower = more prediction (divisor)
        inline float prediction_y = 15.0f;  // Usually want less Y prediction
        inline bool prediction_ignore_y = true;
        inline bool sticky_aim = false;
        inline int target_selection = 0;

        // Air Part (different hitbox when target is jumping)
        inline bool air_part_enabled = false;
        inline int air_part_hitbox = 1; // 0 = Head, 1 = Body

        // Anti-Flick
        inline bool anti_flick = false;
        inline float anti_flick_distance = 500.0f;

        // Smoothing Style
        inline int smoothing_style = 1; // 0 = None, 1 = Linear, 2 = EaseIn, 3 = EaseOut, 4 = EaseInOut

        // Shake (humanization)
        inline bool shake = false;
        inline float shake_x = 2.0f;
        inline float shake_y = 2.0f;

        // Unlock on death
        inline bool unlock_on_death = true;
    }

    namespace combat {
        inline bool hitbox_expander = false;
        inline float hitbox_size_x = 8.0f;
        inline float hitbox_size_y = 8.0f;
        inline float hitbox_size_z = 8.0f;
        inline float hitbox_multiplier = 2.0f;
        inline bool hitbox_visible = false;
        inline bool hitbox_can_collide = false;
        inline bool hitbox_skip_teammates = true;
    }

    namespace speed_hack
    {
        inline bool toggled = false;
        inline float value = 16.0f;
    }

    namespace freecam
    {
        inline bool toggled = false;
        inline float speed = 2.0f;
        inline float sensitivity = 0.003f;
    }

    namespace set_fov 
    {
        inline float set_fov = 70.0f;
        inline bool toggled = false;
        inline bool unlock_zoom = false;
    }

    namespace noclip
    {
        inline bool toggled = false;
    }

    namespace jump_power
    {
        inline bool toggled = false;
        inline float value = 50.0f;
        inline float default_value = 50.0f;
    }

    namespace infinite_jump
    {
        inline bool toggled = false;
        inline float jump_power_value = 5000.0f;
    }

    namespace fly
    {
        inline bool toggled = false;
        inline float speed = 1.0f;
        inline int fly_toggle_key = 0x51; // Q key
        inline int fly_mode = 0; // 0 = Hold, 1 = Toggle
    }

    namespace airswim
    {
        inline bool toggled = false;
    }

    namespace misc
    {
        inline bool show_workspace_viewer = false;
        inline float teleport_offset_x = 0.0f;
        inline float teleport_offset_y = 3.0f;  // Default slight Y offset to spawn above target
        inline float teleport_offset_z = 0.0f;
        inline uintptr_t selected_player_for_info = 0;
        inline std::string spectating_player_name = "";
        inline uintptr_t spectating_camera = 0;
    }

    // Desync controls
    namespace desync
    {
        inline bool enabled = false; // Master toggle for desync feature
        // When true, writes a float value that corresponds to "yes" in the python script
        inline bool desync_on = false; // true = yes, false = no
        // Offset from module base
        inline uintptr_t offset = 0x69F6E10;
        // Sleep interval in milliseconds for writes
        inline int interval_ms = 200;
    }

    namespace bss
    {
        struct ServerEntry {
            std::string job_id;
            std::chrono::steady_clock::time_point visit_time;
        };

        inline std::vector<std::string> blacklisted_friends;
        inline char friend_input_buffer[64] = "";
        inline bool vicious_hunter = false;
        inline bool vicious_found = false;
        inline bool is_hopping = false;
        inline bool vicious_esp = true;
        inline int servers_checked = 0;
        inline float check_delay = 5.0f;
        inline float server_load_delay = 15.0f;
        inline std::vector<ServerEntry> visited_servers;  // Changed from vector<string>
        inline std::string current_job_id = "";
        inline bool avoid_friends = true;
        inline float server_blacklist_time = 300.0f; // 5 minutes default
        
        inline bool auto_teleport = false;
        inline bool float_to_vicious = false;
        inline float float_speed = 50.0f;
        inline bool is_floating = false;
        
        // USE FLOATS ONLY - Delete "inline vector target_pos..."
        inline float target_x = 0.0f;
        inline float target_y = 0.0f;
        inline float target_z = 0.0f;
        
        inline bool webhook_enabled = false;
        inline std::string webhook_url = "";
        inline bool need_hive_first = true;      // Enable going to hive first
        inline bool going_to_hive = false;       // Currently going to hive
        inline bool hive_claimed = false;        // Already got hive
        inline float hive_wait_time = 3.0f;      // Seconds to wait at hive
        inline float post_hop_delay = 11.0f;
        extern float stored_vicious_x;
        extern float stored_vicious_y;
        extern float stored_vicious_z;
        inline bool test_hive_claim = false;
        inline int vicious_kills = 0;
        inline float session_start_time = 0;
        inline bool session_active = false;

        // Vicious tracking
        inline bool vicious_alive = false;
        inline bool tracking_vicious = false;
    }

    namespace anti_afk
    {
        inline bool toggled = false;
        inline float interval = 15.0f;
    }

    namespace lag_switch
    {
        inline bool toggled = false;
        inline int activation_key = 0x4C; // L key
        inline float lag_duration = 0.5f; // How long to lag (seconds)
        inline float lag_interval = 2.0f; // How often to lag (seconds)
        inline bool auto_lag = false; // Automatically lag at intervals
        inline bool manual_lag = true; // Manual lag on key press
    }

    // New exploits namespace
    namespace exploits
    {
        inline bool enabled = false; // Master toggle for exploits
        inline bool friendly_fire = false; // Toggle to enable friendly fire
    }
}