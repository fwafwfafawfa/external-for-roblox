// fly.cpp – pro Humanoid.MoveDirection fly, exactly your style
#include "fly.hpp"
#include "../../../handlers/vars.hpp"
#include "../../../game/offsets/offsets.hpp"
#include "../../../game/core.hpp"
#include "../../../addons/kernel/memory.hpp"

static bool fly_toggled = false;

void c_fly::run()
{
    if (!g_main::localplayer) return;
    if (!vars::fly::toggled) return;

    bool should_fly = false;

    if (vars::fly::fly_mode == 0) // Hold mode
    {
        should_fly = GetAsyncKeyState(vars::fly::fly_toggle_key) & 0x8000;
    }
    else if (vars::fly::fly_mode == 1) // Toggle mode
    {
        if (GetAsyncKeyState(vars::fly::fly_toggle_key) & 0x1)
        {
            fly_toggled = !fly_toggled;
        }
        should_fly = fly_toggled;
    }

    if (!should_fly) return;

    uintptr_t character = core.get_model_instance(g_main::localplayer);
    if (!character) return;

    uintptr_t humanoidRootPart = core.find_first_child(character, "HumanoidRootPart");
    if (!humanoidRootPart) return;

    uintptr_t humanoid = core.find_first_child(character, "Humanoid");
    if (!humanoid) return;

    uintptr_t primitive = memory->read<uintptr_t>(humanoidRootPart + offsets::Primitive);
    if (!primitive) return;

    // Read Humanoid.MoveDirection (this is the  magic – already camera-relative!)
    vector moveDirection = memory->read<vector>(humanoid + offsets::MoveDirection);

    // Up/Down input
    float upInput = 0.0f;
    if (GetAsyncKeyState(VK_SPACE) & 0x8000)   upInput += 1.0f;
    if (GetAsyncKeyState(VK_CONTROL) & 0x8000) upInput -= 1.0f;

    // Get world UP vector from current character orientation
    Matrix3 cf = memory->read<Matrix3>(primitive + offsets::CFrame);
    vector upVector = { cf.data[1], cf.data[4], cf.data[7] }; // Y column = up

    // Normalize up vector
    float upLen = sqrtf(upVector.x * upVector.x + upVector.y * upVector.y + upVector.z * upVector.z);
    if (upLen > 0.001f)
    {
        upVector.x /= upLen;
        upVector.y /= upLen;
        upVector.z /= upLen;
    }

    // Final movement + vertical
    vector finalDir;
    finalDir.x = moveDirection.x + upVector.x * upInput;
    finalDir.y = moveDirection.y + upVector.y * upInput;
    finalDir.z = moveDirection.z + upVector.z * upInput;

    // Normalize and apply speed
    float mag = sqrtf(finalDir.x * finalDir.x + finalDir.y * finalDir.y + finalDir.z * finalDir.z);

    if (mag > 0.001f)
    {
        float speedMultiplier = vars::fly::speed * 25.0f;

        vector velocity;
        velocity.x = (finalDir.x / mag) * speedMultiplier;
        velocity.y = (finalDir.y / mag) * speedMultiplier;
        velocity.z = (finalDir.z / mag) * speedMultiplier;

        memory->write<vector>(primitive + offsets::Velocity, velocity);
    }
    else
    {
        memory->write<vector>(primitive + offsets::Velocity, vector{ 0,0,0 });
    }

    // Noclip
    memory->write<bool>(primitive + offsets::CanCollide, false);
}