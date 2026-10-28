#pragma once

#include "Primitives.hpp"

namespace Crowy
{
    class InputProvider;

    // a held right button turns by the mouse; true when it turned
    bool flyLook(
        const InputProvider& input,
        f32 sensitivity,
        f32& yaw,
        f32& pitch
    );

    // WASD, Space and Shift: x to the right, y up, z forward
    Vec3 flyMoveInput(const InputProvider& input);
    // the wheel scales the walk's speed, a notch at a time, from `scale`
    f32 flySpeedScale(const InputProvider& input, f32 scale);

    Vec4 flyRotation(f32 yaw, f32 pitch);
    Vec3 flyMove(
        Vec3 position,
        Vec4 rotation,
        Vec3 moveInput,
        f32 speed,
        f64 seconds
    );

    Transform flyStep(
        const Transform& local,
        f32 yaw,
        f32 pitch,
        Vec3 moveInput,
        f32 speed,
        f64 seconds
    );
}
