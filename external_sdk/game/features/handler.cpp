#include "handler.hpp"
#include "example_esp/esp.hpp"
#include "speed_hack/speed.hpp"
#include "freecam/freecam.hpp"
#include "fly/fly.hpp"
#include "infinite_jump/infinite_jump.hpp"
#include "airswim/airswim.hpp"
#include "../../tphandler.hpp"

#include "../../handlers/vars.hpp"
#include "../../handlers/misc/misc.hpp"
#include <chrono>

void c_feature_handler::start(uintptr_t datamodel)
{
    if (!memory) return;
    if (!g_main::datamodel) return;
    if (!g_main::localplayer) return;
    if (!tp_handler.is_ready()) return;

    static auto last_time = std::chrono::high_resolution_clock::now();
    auto now = std::chrono::high_resolution_clock::now();
    std::chrono::duration<float> dt = now - last_time;
    last_time = now;

    if (!datamodel && !g_main::v_engine) return;

    static HWND roblox_window = NULL;
    static std::chrono::steady_clock::time_point last_window_check;
    float ms_since_window_check = std::chrono::duration<float, std::milli>(now - last_window_check).count();

    if (ms_since_window_check > 1000.0f || roblox_window == NULL)
    {
        last_window_check = now;
        roblox_window = FindWindowA(NULL, "Roblox");
        if (!roblox_window) roblox_window = FindWindowA("WINDOWSCLIENT", NULL);
    }


    // ===== ONLY IF FOCUSED =====
    bool roblox_focused = (roblox_window && GetForegroundWindow() == roblox_window);
    if (!roblox_focused) return;

    if (!g_main::v_engine) return;

    view_matrix_t viewmatrix = memory->read<view_matrix_t>(g_main::v_engine + offsets::viewmatrix);

    if (vars::esp::toggled)
        esp.run_players(viewmatrix);

    esp.run_aimbot(viewmatrix);
    esp.draw_hitbox_esp(viewmatrix);

    speed_hack::run();
    freecam.enabled = vars::freecam::toggled;
    freecam.run(dt.count());
    jump_power::run();
    infinite_jump::run();
    airswim.run();

    if (vars::fly::toggled)
        fly.run();

    if (vars::set_fov::unlock_zoom)
        freecam.unlock_zoom();

    if (vars::set_fov::toggled)
        freecam.set_fov(vars::set_fov::set_fov);
}