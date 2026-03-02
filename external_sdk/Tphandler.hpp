#pragma once
#include "main.hpp"
#include <thread>
#include <atomic>
#include <chrono>
class c_tp_handler {
private:
    std::atomic<bool> running = false;
    std::thread monitor_thread;

    uint64_t last_job_id = 0;
    uint64_t last_place_id = 0;

    std::atomic<bool> force_rescan_flag = false;
    std::atomic<bool> is_hopping = false;
public:
    void request_rescan() {
        is_hopping = true;
        force_rescan_flag = true;
        util.m_print("tp_handler: Rescan requested");
    }

    bool is_ready() const {
        if (is_hopping) return false;
        if (g_safe_mode) return false;
        if (!memory) return false;
        if (g_main::datamodel == 0) return false;
        if (g_main::localplayer == 0) return false;
        if (g_main::v_engine == 0) return false;
        return true;
    }

    void update_cache() {
        if (g_main::datamodel != 0) {
            last_job_id = memory->read<uint64_t>(g_main::datamodel + offsets::JobId);
            last_place_id = memory->read<uint64_t>(g_main::datamodel + offsets::PlaceId);
        }
    }

    void clear_pointers() {
        g_main::datamodel = 0;
        g_main::localplayer = 0;
        g_main::v_engine = 0;
        g_main::localplayer_team = 0;
        last_job_id = 0;
        last_place_id = 0;
    }

    void monitor_loop() {
        util.m_print("tp_handler: Monitor started");

        while (running) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));

            try {
                if (is_hopping) {
                    util.m_print("tp_handler: [HOP] Hopping mode active...");

                    // Clear memory
                    if (memory) {
                        util.m_print("tp_handler: [HOP] Deleting old memory object...");
                        delete memory;
                        memory = nullptr;
                    }
                    g_main::datamodel = 0;
                    g_main::localplayer = 0;
                    g_main::v_engine = 0;
                    last_job_id = 0;

                    // Wait for Roblox to close
                    util.m_print("tp_handler: [HOP] Waiting for Roblox to close...");
                    while (FindWindowA(NULL, "Roblox")) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    }
                    util.m_print("tp_handler: [HOP] Roblox closed!");

                    // SAFE MODE OFF HERE
                    g_safe_mode = false;
                    util.m_print("tp_handler: [HOP] Safe mode disabled");

                    // Wait for new window
                    util.m_print("tp_handler: [HOP] Waiting for new Roblox window...");
                    int timeout = 0;
                    while (!FindWindowA(NULL, "Roblox") && running && timeout < 120) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(500));
                        timeout++;
                    }

                    if (!running) break;

                    if (timeout >= 120) {
                        util.m_print("tp_handler: [HOP] Timeout waiting for window!");
                        is_hopping = false;
                        continue;
                    }

                    util.m_print("tp_handler: [HOP] New window found! Waiting for game to load...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(10000));  // 15 seconds for BSS

                    is_hopping = false;
                    force_rescan_flag = true;
                    util.m_print("tp_handler: [HOP] Ready to reattach!");
                }

                // ====== CHECK WINDOW EXISTS ======
                HWND hwnd = FindWindowA(NULL, "Roblox");
                if (!hwnd) {
                    if (memory || g_main::datamodel != 0) {
                        util.m_print("tp_handler: Window gone, resetting...");
                        if (memory) { delete memory; memory = nullptr; }
                        clear_pointers();
                        g_safe_mode = false;
                    }
                    continue;
                }

                // ====== ENSURE SAFE MODE IS OFF ======
                g_safe_mode = false;

                // ====== CREATE MEMORY IF NEEDED ======
                if (!memory) {
                    util.m_print("tp_handler: Creating memory object...");
                    std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                    memory = new c_memory("RobloxPlayerBeta.exe");

                    if (!memory || !memory->find_image()) {
                        util.m_print("tp_handler: Failed to attach!");
                        if (memory) { delete memory; memory = nullptr; }
                        continue;
                    }

                    util.m_print("tp_handler: Memory attached!");
                    force_rescan_flag = true;
                }

                if (g_main::datamodel != 0 && memory) {
                    auto base_address = memory->find_image();
                    if (base_address) {
                        // Re-read current DataModel from memory
                        uintptr_t fake_dm_ptr = memory->read<uintptr_t>(base_address + offsets::FakeDataModelPointer);
                        uintptr_t current_datamodel = 0;

                        if (fake_dm_ptr && fake_dm_ptr > 0x10000) {
                            current_datamodel = memory->read<uintptr_t>(fake_dm_ptr + offsets::FakeDataModelToDataModel);
                        }

                        // DataModel address changed = game switched
                        if (current_datamodel != g_main::datamodel) {
                            util.m_print("tp_handler: DataModel changed! Reinitializing...");
                            clear_pointers();
                            force_rescan_flag = true;
                        }
                        // DataModel invalid
                        else if (current_datamodel == 0 || current_datamodel < 0x10000) {
                            util.m_print("tp_handler: DataModel invalid!");
                            clear_pointers();
                            force_rescan_flag = true;
                        }
                    }
                }
                // ====== INITIALIZE POINTERS IF NEEDED ======
                bool needs_init = force_rescan_flag ||
                    g_main::datamodel == 0 ||
                    g_main::localplayer == 0;

                if (needs_init) {
                    util.m_print("tp_handler: Initializing pointers...");
                    force_rescan_flag = false;

                    for (int attempt = 0; attempt < 10; attempt++) {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1000));

                        reinitialize_game_pointers();

                        if (g_main::datamodel != 0 && g_main::localplayer != 0) {
                            update_cache();
                            util.m_print("tp_handler: SUCCESS! Pointers found.");
                            break;
                        }

                        util.m_print("tp_handler: Attempt %d/10 failed...", attempt + 1);
                    }

                    if (g_main::datamodel == 0) {
                        util.m_print("tp_handler: Failed after 10 attempts!");
                    }
                }

            }
            catch (...) {
                util.m_print("tp_handler: Exception caught, resetting...");
                if (memory) { delete memory; memory = nullptr; }
                clear_pointers();
                g_safe_mode = false;
            }
        }

        util.m_print("tp_handler: Monitor stopped");
    }

    void start() {
        if (running) return;
        running = true;

        reinitialize_game_pointers();
        update_cache();

        monitor_thread = std::thread(&c_tp_handler::monitor_loop, this);
        monitor_thread.detach();

        util.m_print("tp_handler: Started");
    }

    void stop() {
        running = false;
    }
};
inline c_tp_handler tp_handler;
