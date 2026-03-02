#include "noclip.hpp"
#include "../game/instance.hpp"
#include <thread>
#include <atomic>

static std::atomic<bool> s_thread_running{ false };
static std::atomic<bool> s_should_stop{ false };

void noclip_thread_func()
{
    uintptr_t prims[30];
    int prim_count = 0;
    uintptr_t last_char = 0;

    while (!s_should_stop)
    {
        bool want = vars::noclip::toggled;

        if (!want || !g_main::localplayer)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        uintptr_t character = memory->read<uintptr_t>(g_main::localplayer + offsets::Player::ModelInstance);

        if (character && character != last_char)
        {
            last_char = character;
            prim_count = 0;

            uintptr_t children_ptr = memory->read<uintptr_t>(character + offsets::Instance::ChildrenStart);
            if (children_ptr)
            {
                uintptr_t start = memory->read<uintptr_t>(children_ptr);
                uintptr_t end = memory->read<uintptr_t>(children_ptr + offsets::Instance::ChildrenEnd);

                if (start && end && end > start)
                {
                    size_t count = (end - start) / 8;
                    if (count > 30) count = 30;

                    for (size_t i = 0; i < count; i++)
                    {
                        uintptr_t child = memory->read<uintptr_t>(start + i * 8);
                        if (!child) continue;

                        uintptr_t prim = memory->read<uintptr_t>(child + offsets::BasePart::Primitive);
                        if (prim) prims[prim_count++] = prim;
                    }
                }
            }
        }

        // Apply noclip
        for (int i = 0; i < prim_count; i++)
        {
            uint8_t flags = memory->read<uint8_t>(prims[i] + 0x1ae);
            memory->write<uint8_t>(prims[i] + 0x1ae, flags & 0xF7);
        }

        // Sleep 50ms - adjust this for smoothness vs CPU usage
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    s_thread_running = false;
}

void c_noclip::run()
{
    bool want = vars::noclip::toggled;

    if (want && !s_thread_running)
    {
        s_should_stop = false;
        s_thread_running = true;
        std::thread(noclip_thread_func).detach();
    }
    else if (!want && s_thread_running)
    {
        s_should_stop = true;
    }

    // Main thread does nothing - all work in background
}