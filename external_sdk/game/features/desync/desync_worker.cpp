#include "desync_worker.hpp"
#include "desync.hpp"
#include "../../../addons/kernel/memory.hpp"
#include "../../../game/offsets/offsets.hpp"
#include "../../../handlers/vars.hpp"
#include "../../../main.hpp"
#include <chrono>

static std::atomic<bool> s_running{ false };
static std::thread s_thread;

void desync_worker::start()
{
    if (s_running) return;
    s_running = true;
    s_thread = std::thread([]()
    {
        auto base = memory->find_image();
        while (s_running)
        {
            if (!base) base = memory->find_image();

            if (!base)
            {
                std::this_thread::sleep_for(std::chrono::milliseconds(500));
                continue;
            }

            uintptr_t address = base + vars::desync::offset;

            // Sanity check: ensure address is readable/writable by trying to read current float
            float cur = 0.0f;
            bool ok = memory->try_read<float>(address, cur);
            if (!ok)
            {
                // If we can't read, don't write - wait and retry
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
                continue;
            }

            // Only write if value differs to reduce churn
            float target = vars::desync::desync_on ? 0.0f : 5.4314328e-41f;
            if (cur != target)
            {
                // Try write and verify
                bool wrote = memory->write<float>(address, target);
                if (!wrote)
                {
                    // If write failed, back off
                    std::this_thread::sleep_for(std::chrono::milliseconds(500));
                    continue;
                }

                // Verify write
                float verify = 0.0f;
                if (!memory->try_read<float>(address, verify) || verify != target)
                {
                    // If verification fails, disable worker to be safe
                    s_running = false;
                    return;
                }
            }

            // Sleep according to configured interval but enforce a minimum
            int interval = vars::desync::interval_ms;
            if (interval < 50) interval = 50;
            std::this_thread::sleep_for(std::chrono::milliseconds(interval));
        }
    });
}

void desync_worker::stop()
{
    if (!s_running) return;
    s_running = false;
    if (s_thread.joinable()) s_thread.join();
}

bool desync_worker::running()
{
    return s_running.load();
}
