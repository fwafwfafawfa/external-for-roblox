#include "desync.hpp"
#include "desync_worker.hpp"
#include "../../../handlers/vars.hpp"
#include "../../../addons/kernel/memory.hpp"
#include "../../../main.hpp"
#include <thread>
#include <atomic>

namespace {
    static std::atomic<bool> g_desync_thread_running{ false };

    bool try_call_raknet_desync()
    {
        // 1. Check for the Module
        HMODULE hRak = GetModuleHandleA("RakNet.dll");
        if (!hRak) hRak = GetModuleHandleA("Raknet.dll");

        // CHANGE: Check main module if DLL not found (Common in Minecraft)
        if (!hRak) hRak = GetModuleHandleA(NULL);

        if (!hRak) {
            // This shouldn't happen if checking NULL, but good for safety
            util.m_print("desync: Could not find RakNet module or Main Module.");
            return false;
        }

        // 2. Check for the Function
        // Note: If this is standard RakNet, THESE WILL ALL FAIL.
        using desync_fn_t = void(__cdecl*)(bool);
        desync_fn_t fn = (desync_fn_t)GetProcAddress(hRak, "desync");

        // Debug print specifically which step failed
        if (!fn) {
            // Remove this print if it spams your console, but it proves the function is missing
            // util.m_print("desync: DLL found, but function 'desync' is missing."); 
            return false;
        }

        __try {
            fn(true);
            util.m_print("desync: Success!");
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER) {
            util.m_print("desync: Crashed while calling function.");
            return false;
        }
    }
}

namespace hooks {
    void desync()
    {
        util.m_print("desync: attempting to call RakNet.desync(true)");

        // First try immediately
        if (try_call_raknet_desync())
            return;

        util.m_print("desync: RakNet module not loaded, scheduling background poll...");

        // If a background poll is already running, do not start another
        bool expected = false;
        if (!g_desync_thread_running.compare_exchange_strong(expected, true)) {
            util.m_print("desync: poll already running, will not start another");
            return;
        }

        // Start a background thread to poll for the module and call the function when available
        std::thread([]() {
            const int max_attempts = 120; // try up to 120 times (~60 seconds at 500ms sleep)
            int attempt = 0;
            while (attempt < max_attempts) {
                if (try_call_raknet_desync())
                    break;

                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                attempt++;
            }

            if (attempt >= max_attempts) {
                util.m_print("desync: timed out waiting for RakNet module");
            }

            g_desync_thread_running = false;
        }).detach();
    }
}

namespace desync_feature {
    void enable(bool on)
    {
        vars::desync::desync_on = on;
        if (on) start_worker();
    }

    void start_worker()
    {
        if (!desync_worker::running())
            desync_worker::start();
    }

    void stop_worker()
    {
        if (desync_worker::running())
            desync_worker::stop();
    }
}
