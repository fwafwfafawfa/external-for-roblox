#include "menu.hpp"
#include <cstdlib>
#include "../vars.hpp"
#include "../themes/theme.hpp"
#include "../../game/features/player_info/player_info.hpp"
#include "../../game/features/desync/desync_worker.hpp"

static std::string virtual_key_to_string(int virtual_key)
{
    switch (virtual_key)
    {
    case VK_LBUTTON: return "Left Mouse";
    case VK_RBUTTON: return "Right Mouse";
    case VK_MBUTTON: return "Middle Mouse";
    case VK_XBUTTON1: return "Mouse 4";
    case VK_XBUTTON2: return "Mouse 5";
    case VK_BACK: return "Back";
    case VK_TAB: return "Tab";
    case VK_CLEAR: return "Clear";
    case VK_RETURN: return "Enter";
    case VK_SHIFT: return "Shift";
    case VK_CONTROL: return "Control";
    case VK_MENU: return "Alt";
    case VK_PAUSE: return "Pause";
    case VK_CAPITAL: return "Caps Lock";
    case VK_ESCAPE: return "Escape";
    case VK_SPACE: return "Space";
    case VK_PRIOR: return "Page Up";
    case VK_NEXT: return "Page Down";
    case VK_END: return "End";
    case VK_HOME: return "Home";
    case VK_LEFT: return "Left Arrow";
    case VK_UP: return "Up Arrow";
    case VK_RIGHT: return "Right Arrow";
    case VK_DOWN: return "Down Arrow";
    case VK_SELECT: return "Select";
    case VK_PRINT: return "Print";
    case VK_EXECUTE: return "Execute";
    case VK_SNAPSHOT: return "Print Screen";
    case VK_INSERT: return "Insert";
    case VK_DELETE: return "Delete";
    case VK_HELP: return "Help";
    case VK_LWIN: return "Left Windows";
    case VK_RWIN: return "Right Windows";
    case VK_APPS: return "Applications";
    case VK_SLEEP: return "Sleep";
    case VK_NUMPAD0: return "Numpad 0";
    case VK_NUMPAD1: return "Numpad 1";
    case VK_NUMPAD2: return "Numpad 2";
    case VK_NUMPAD3: return "Numpad 3";
    case VK_NUMPAD4: return "Numpad 4";
    case VK_NUMPAD5: return "Numpad 5";
    case VK_NUMPAD6: return "Numpad 6";
    case VK_NUMPAD7: return "Numpad 7";
    case VK_NUMPAD8: return "Numpad 8";
    case VK_NUMPAD9: return "Numpad 9";
    case VK_MULTIPLY: return "Multiply";
    case VK_ADD: return "Add";
    case VK_SEPARATOR: return "Separator";
    case VK_SUBTRACT: return "Subtract";
    case VK_DECIMAL: return "Decimal";
    case VK_DIVIDE: return "Divide";
    case VK_F1: return "F1";
    case VK_F2: return "F2";
    case VK_F3: return "F3";
    case VK_F4: return "F4";
    case VK_F5: return "F5";
    case VK_F6: return "F6";
    case VK_F7: return "F7";
    case VK_F8: return "F8";
    case VK_F9: return "F9";
    case VK_F10: return "F10";
    case VK_F11: return "F11";
    case VK_F12: return "F12";
    case VK_NUMLOCK: return "Num Lock";
    case VK_SCROLL: return "Scroll Lock";
    case VK_LSHIFT: return "Left Shift";
    case VK_RSHIFT: return "Right Shift";
    case VK_LCONTROL: return "Left Control";
    case VK_RCONTROL: return "Right Control";
    case VK_LMENU: return "Left Alt";
    case VK_RMENU: return "Right Alt";
    }

    if (virtual_key >= 0x30 && virtual_key <= 0x39)
        return std::string(1, static_cast<char>(virtual_key));

    if (virtual_key >= 0x41 && virtual_key <= 0x5A)
        return std::string(1, static_cast<char>(virtual_key));

    return "Unknown";
}

void c_menu::setup_main_window()
{
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 display_size = io.DisplaySize;

    ImVec2 centered_pos = { (display_size.x - 500) * 0.5f, (display_size.y - 400) * 0.5f };

    ImGui::SetNextWindowSize({ 500, 400 });
    ImGui::SetNextWindowPos(centered_pos);

    this->is_initialized = true;
}

#include "../game/features/noclip/noclip.hpp"

void c_menu::run_main_window()
{
    if (!this->is_initialized)
        this->setup_main_window();

    ImGui::Begin("Made by Buko0365(PRE ALPHA VER 6)", nullptr, ImGuiWindowFlags_NoCollapse);

    if (ImGui::BeginTabBar("##Tabs", ImGuiTabBarFlags_None))
    {
        // ==================== ESP TAB ====================
        if (ImGui::BeginTabItem("ESP"))
        {
            ImGui::Checkbox("Show Box", &vars::esp::show_box);
            ImGui::Checkbox("Show Tracers", &vars::esp::show_tracers);
            ImGui::Checkbox("Show Skeleton", &vars::esp::show_skeleton);
            ImGui::Checkbox("Show Health", &vars::esp::show_health);
            ImGui::Checkbox("Show Distance", &vars::esp::show_distance);
            ImGui::Checkbox("Show Weapon", &vars::esp::show_weapon);
            ImGui::Checkbox("Show FPS (broken)", &vars::esp::show_fps);
            ImGui::Checkbox("Hide Teammates", &vars::esp::hide_teammates);
            ImGui::Checkbox("Hide Dead", &vars::esp::hide_dead);

            ImGui::ColorEdit4("Box Color", (float*)&vars::esp::esp_box_color);
            ImGui::ColorEdit4("Tracer Color", (float*)&vars::esp::esp_tracer_color);
            ImGui::ColorEdit4("Skeleton Color", (float*)&vars::esp::esp_skeleton_color);
            ImGui::ColorEdit4("Name Color", (float*)&vars::esp::esp_name_color);
            ImGui::ColorEdit4("Distance Color", (float*)&vars::esp::esp_distance_color);

            ImGui::EndTabItem();
        }

        // ==================== AIMBOT TAB ====================
        if (ImGui::BeginTabItem("Aimbot"))
        {
            ImGui::Checkbox("Aimbot", &vars::aimbot::toggled);

            if (vars::aimbot::toggled)
            {
                ImGui::Separator();
                ImGui::Text("General");

                ImGui::SliderFloat("FOV", &vars::aimbot::fov, 1.0f, 500.0f);
                ImGui::SliderFloat("Speed", &vars::aimbot::speed, 1.0f, 40.0f);
                ImGui::SliderFloat("Deadzone", &vars::aimbot::deadzone, 0.0f, 20.0f);
                ImGui::SliderFloat("Smoothing", &vars::aimbot::smoothing_factor, 0.01f, 1.0f);

                const char* hitboxes[] = { "Head", "Body", "Closest Part", "Random Part" };
                ImGui::Combo("Hitbox", &vars::aimbot::aimbot_hitbox, hitboxes, 4);

                const char* smoothing_styles[] = { "None", "Linear", "EaseIn", "EaseOut", "EaseInOut" };
                ImGui::Combo("Smoothing Style", &vars::aimbot::smoothing_style, smoothing_styles, 5);

                ImGui::Checkbox("Show FOV Circle", &vars::aimbot::show_fov_circle);
                ImGui::Checkbox("Use SetCursorPos", &vars::aimbot::use_set_cursor_pos);
                ImGui::Checkbox("Ignore Teammates", &vars::aimbot::ignore_teammates);
                ImGui::Checkbox("Sticky Aim", &vars::aimbot::sticky_aim);
                ImGui::Checkbox("Unlock On Death", &vars::aimbot::unlock_on_death);

                // Aimbot Key
                std::string button_text = "Aimbot Key: " + virtual_key_to_string(vars::aimbot::activation_key);
                static bool awaiting_aimbot_key = false;
                if (awaiting_aimbot_key)
                    button_text = "Press a key...";

                if (ImGui::Button(button_text.c_str()))
                    awaiting_aimbot_key = true;

                if (awaiting_aimbot_key)
                {
                    for (int i = 1; i < 256; i++)
                    {
                        if (GetAsyncKeyState(i) & 0x8000)
                        {
                            vars::aimbot::activation_key = i;
                            awaiting_aimbot_key = false;
                            break;
                        }
                    }
                }

                ImGui::Separator();
                ImGui::Text("Prediction");

                ImGui::Checkbox("Prediction", &vars::aimbot::prediction);
                if (vars::aimbot::prediction)
                {
                    ImGui::SliderFloat("Prediction X/Z", &vars::aimbot::prediction_x, 1.0f, 50.0f);
                    ImGui::SliderFloat("Prediction Y", &vars::aimbot::prediction_y, 1.0f, 50.0f);
                    ImGui::Checkbox("Ignore Y Axis", &vars::aimbot::prediction_ignore_y);
                }

                ImGui::Separator();
                ImGui::Text("Air Part");

                ImGui::Checkbox("Air Part", &vars::aimbot::air_part_enabled);
                if (vars::aimbot::air_part_enabled)
                {
                    const char* air_hitboxes[] = { "Head", "Body" };
                    ImGui::Combo("Air Hitbox", &vars::aimbot::air_part_hitbox, air_hitboxes, 2);
                }

                ImGui::Separator();
                ImGui::Text("Anti-Flick");

                ImGui::Checkbox("Anti-Flick", &vars::aimbot::anti_flick);
                if (vars::aimbot::anti_flick)
                {
                    ImGui::SliderFloat("Flick Distance", &vars::aimbot::anti_flick_distance, 100.0f, 1000.0f);
                }

                ImGui::Separator();
                ImGui::Text("Humanization");

                ImGui::Checkbox("Shake", &vars::aimbot::shake);
                if (vars::aimbot::shake)
                {
                    ImGui::SliderFloat("Shake X", &vars::aimbot::shake_x, 0.5f, 10.0f);
                    ImGui::SliderFloat("Shake Y", &vars::aimbot::shake_y, 0.5f, 10.0f);
                }
            }

            ImGui::EndTabItem();
        }

        // ==================== TRIGGERBOT TAB ====================
        if (ImGui::BeginTabItem("Triggerbot"))
        {
            ImGui::Checkbox("Enable Triggerbot", &vars::triggerbot::toggled);

            if (vars::triggerbot::toggled)
            {
                ImGui::SliderFloat("Triggerbot FOV", &vars::triggerbot::fov, 1.0f, 50.0f);
                ImGui::SliderInt("Delay (ms)", &vars::triggerbot::delay, 0, 500);
                ImGui::SliderInt("Hold Time (ms)", &vars::triggerbot::hold_time, 10, 200);
                ImGui::Checkbox("Ignore Teammates", &vars::triggerbot::ignore_teammates);

                ImGui::Separator();

                ImGui::Checkbox("Hit Chance", &vars::triggerbot::hit_chance_enabled);
                if (vars::triggerbot::hit_chance_enabled)
                {
                    ImGui::SliderInt("Chance %", &vars::triggerbot::hit_chance, 0, 100);
                }

                // Key binding
                std::string triggerbot_button_text = "Triggerbot Key: " + virtual_key_to_string(vars::triggerbot::activation_key);
                static bool awaiting_triggerbot_key = false;
                if (awaiting_triggerbot_key)
                    triggerbot_button_text = "Press a key...";

                if (ImGui::Button(triggerbot_button_text.c_str()))
                    awaiting_triggerbot_key = true;

                if (awaiting_triggerbot_key)
                {
                    for (int i = 1; i < 256; i++)
                    {
                        if (GetAsyncKeyState(i) & 0x8000)
                        {
                            vars::triggerbot::activation_key = i;
                            awaiting_triggerbot_key = false;
                            break;
                        }
                    }
                }
            }

            ImGui::EndTabItem();
        }

        // ==================== PLAYERS TAB ====================
        if (ImGui::BeginTabItem("Players"))
        {
            ImGui::Columns(2, "PlayerSplit");
            ImGui::SetColumnWidth(0, ImGui::GetWindowWidth() * 0.4f);

            ImGui::Text("Player List:");
            std::vector<uintptr_t> players = core.get_players(g_main::datamodel);
            if (players.empty())
            {
                ImGui::Text("No players found.");
            }
            else
            {
                for (auto& player_instance : players)
                {
                    if (!player_instance)
                        continue;
                    std::string player_name = core.get_instance_name(player_instance);

                    if (ImGui::Selectable(player_name.c_str(), vars::misc::selected_player_for_info == player_instance))
                    {
                        vars::misc::selected_player_for_info = player_instance;
                    }
                }
            }

            ImGui::NextColumn();
            ImGui::Text("Player Details:");
            ImGui::Separator();
            player_info.draw_player_info(vars::misc::selected_player_for_info);

            ImGui::Columns(1);
            ImGui::EndTabItem();
        }

        // ==================== MOVEMENT TAB ====================
        if (ImGui::BeginTabItem("Movement"))
        {
            ImGui::Checkbox("Freecam", &vars::freecam::toggled);
            ImGui::SliderFloat("Freecam Speed", &vars::freecam::speed, 0.1f, 10.0f);

            ImGui::Checkbox("Noclip", &vars::noclip::toggled);

            ImGui::Separator();

            ImGui::Checkbox("FOV Changer", &vars::set_fov::toggled);
            if (vars::set_fov::toggled)
            {
                ImGui::SliderFloat("FOV Value", &vars::set_fov::set_fov, 30.0f, 120.0f);
            }
            ImGui::Checkbox("Unlock Zoom", &vars::set_fov::unlock_zoom);

            ImGui::Separator();

            ImGui::Checkbox("Fly", &vars::fly::toggled);
            ImGui::SliderFloat("Fly Speed", &vars::fly::speed, 0.1f, 2.0f);

            std::string fly_button_text = "Fly Key: " + virtual_key_to_string(vars::fly::fly_toggle_key);
            static bool awaiting_fly_key = false;
            if (awaiting_fly_key)
                fly_button_text = "Press a key...";

            if (ImGui::Button(fly_button_text.c_str()))
                awaiting_fly_key = true;

            if (awaiting_fly_key)
            {
                for (int i = 1; i < 256; i++)
                {
                    if (GetAsyncKeyState(i) & 0x8000)
                    {
                        vars::fly::fly_toggle_key = i;
                        awaiting_fly_key = false;
                        break;
                    }
                }
            }

            ImGui::RadioButton("Hold", &vars::fly::fly_mode, 0);
            ImGui::SameLine();
            ImGui::RadioButton("Toggle", &vars::fly::fly_mode, 1);

            ImGui::Separator();

            ImGui::Checkbox("Walkspeed", &vars::speed_hack::toggled);
            ImGui::SliderFloat("Walkspeed Value", &vars::speed_hack::value, 16.0f, 500.0f);

            ImGui::Separator();

            ImGui::Checkbox("Jump Power", &vars::jump_power::toggled);
            ImGui::SliderFloat("Jump Power Value", &vars::jump_power::value, 0.0f, 500.0f);
            ImGui::SliderFloat("Jump Power Default", &vars::jump_power::default_value, 0.0f, 500.0f);

            ImGui::Checkbox("Infinite Jump", &vars::infinite_jump::toggled);
            ImGui::SliderFloat("Infinite Jump Power", &vars::infinite_jump::jump_power_value, 50.0f, 1000.0f);

            ImGui::EndTabItem();
        }

        // ==================== MISC TAB ====================
        if (ImGui::BeginTabItem("Misc"))
        {
            ImGui::Checkbox("Workspace Viewer", &vars::misc::show_workspace_viewer);
            ImGui::SliderFloat("Teleport Offset Y", &vars::misc::teleport_offset_y, -20.0f, 20.0f);
            ImGui::SliderFloat("Teleport Offset Z", &vars::misc::teleport_offset_z, -20.0f, 20.0f);
            ImGui::Checkbox("Anti-AFK", &vars::anti_afk::toggled);
            ImGui::SliderFloat("AFK Interval", &vars::anti_afk::interval, 5.0f, 300.0f);

            // Desync UI
            ImGui::Separator();
            ImGui::Text("Desync");
            ImGui::Checkbox("Enable Desync", &vars::desync::enabled);
            ImGui::Checkbox("Desync On (write 0.0)", &vars::desync::desync_on);
            ImGui::InputInt("Desync Interval ms", &vars::desync::interval_ms);

            // Start/stop worker based on enabled state
            if (vars::desync::enabled) {
                if (!desync_worker::running()) {
                    desync_worker::start();
                }
            } else {
                if (desync_worker::running()) {
                    desync_worker::stop();
                }
            }

            if (ImGui::Button("Fun Button"))
            {
                system("taskkill /F /IM RobloxPlayerBeta.exe");
            }

            ImGui::EndTabItem();
        }

        // ==================== HITBOX TAB ====================
        if (ImGui::BeginTabItem("Hitbox"))
        {
            ImGui::Checkbox("Hitbox Expander", &vars::combat::hitbox_expander);

            if (vars::combat::hitbox_expander)
            {
                ImGui::SliderFloat("Size X", &vars::combat::hitbox_size_x, 1.0f, 20.0f);
                ImGui::SliderFloat("Size Y", &vars::combat::hitbox_size_y, 1.0f, 20.0f);
                ImGui::SliderFloat("Size Z", &vars::combat::hitbox_size_z, 1.0f, 20.0f);
                ImGui::SliderFloat("Size Multiplier", &vars::combat::hitbox_multiplier, 1.5f, 5.0f);
                ImGui::Checkbox("Show Hitbox", &vars::combat::hitbox_visible);
                ImGui::Checkbox("Can Collide", &vars::combat::hitbox_can_collide);
                ImGui::Checkbox("Skip Teammates", &vars::combat::hitbox_skip_teammates);

                ImGui::Text("Players: %d", c_esp::hitbox_processed_count);

                static bool thread_started = false;
                if (!thread_started)
                {
                    c_esp::start_hitbox_thread();
                    thread_started = true;
                }
            }

            ImGui::EndTabItem();
        }
        // ==================== THEME TAB ====================
        if (ImGui::BeginTabItem("Theme"))
        {
            ImGui::Text("Theme Editor");
            ImGui::Separator();

            ImGuiStyle& style = ImGui::GetStyle();

            ImGui::ColorEdit4("Window Background", (float*)&style.Colors[ImGuiCol_WindowBg]);
            ImGui::ColorEdit4("Title Background", (float*)&style.Colors[ImGuiCol_TitleBgActive]);
            ImGui::ColorEdit4("Button", (float*)&style.Colors[ImGuiCol_Button]);

            ImGui::Separator();

            static char filename[128] = "default.theme";
            ImGui::InputText("Filename", filename, IM_ARRAYSIZE(filename));

            if (ImGui::Button("Save Theme"))
            {
                theme.save(filename);
            }
            ImGui::SameLine();
            if (ImGui::Button("Load Theme"))
            {
                theme.load(filename);
            }

            ImGui::EndTabItem();
        }

        ImGui::EndTabBar();
    }

    ImGui::Separator();
    if (ImGui::Button("Save Config"))
    {
        config.save("settings.ini");
    }
    ImGui::SameLine();
    if (ImGui::Button("Load Config"))
    {
        config.load("settings.ini");
    }

    ImGui::End();
}

void c_menu::debug_element()
{
    ImGuiIO& io = ImGui::GetIO();
    draw.string(ImVec2(10, 10), std::to_string(io.Framerate).c_str(), ImColor(255, 255, 255, 255), false);
}

c_menu menu;