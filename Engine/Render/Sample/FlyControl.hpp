#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    class InputProvider;

    // The free-fly look and walk, shared by FlyCamera and anything else
    // flown the same way, so both move to the same bits.

    // a held right button turns by the mouse; true when it turned
    bool flyLook(
        const InputProvider& input,
        f32 sensitivity,
        f32& yaw,
        f32& pitch
    );

    // WASD, Space and Shift: x to the right, y up, z forward
    Vec3 flyMoveInput(const InputProvider& input);

    Vec4 flyRotation(f32 yaw, f32 pitch);
    // where a walk along moveInput, turned by rotation, ends
    Vec3 flyMove(
        Vec3 position,
        Vec4 rotation,
        Vec3 moveInput,
        f32 speed,
        f64 seconds
    );

    // the local transform a look and a walk leave a flown actor at
    Transform flyStep(
        const Transform& local,
        f32 yaw,
        f32 pitch,
        Vec3 moveInput,
        f32 speed,
        f64 seconds
    );
}
