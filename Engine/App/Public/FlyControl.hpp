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

    // the turn a mouse look keeps apart from the rotation it writes
    struct CameraLook {
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
    };

    // a flown camera's input between ProcessInput and Step, apart from what
    // holds its transform
    class FlyState {
    public:
        CameraLook look;

    private:
        Vec3 moveInput{};
        // the wheel's, on Step's moveSpeed
        f32 speedScale = 1.0f;
        bool looked = false;

    public:
        FlyState() = default;

        explicit FlyState(CameraLook look) noexcept
            : look(look) {}

        // true when it turned
        bool ProcessInput(const InputProvider& input, f32 sensitivity);

        // a turn or a walk: Step changes the transform
        bool Moving() const noexcept;
        Transform Step(
            const Transform& local,
            f32 moveSpeed,
            f64 seconds
        ) const;
    };
}
