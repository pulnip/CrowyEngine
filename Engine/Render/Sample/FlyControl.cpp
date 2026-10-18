#include "FlyControl.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    bool flyLook(
        const InputProvider& input,
        f32 sensitivity,
        f32& yaw,
        f32& pitch
    ) {
        constexpr auto pi_2 = std::numbers::pi_v<f32> / 2;

        if(!input.IsKeyDown(MouseButton::RButton))
            return false;

        const auto dpos = input.GetMouseDPos();

        yaw += dpos.x * sensitivity;
        pitch = std::clamp(
            pitch + dpos.y * sensitivity,
            -pi_2 + 0.01f,
            pi_2 - 0.01f
        );
        return true;
    }

    Vec3 flyMoveInput(const InputProvider& input) {
        return Vec3{
            (input.IsKeyDown(KeyCode::D) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::A) ? 1.0f : 0.0f),
            (input.IsKeyDown(KeyCode::Space) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Shift) ? 1.0f : 0.0f),
            (input.IsKeyDown(KeyCode::W) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::S) ? 1.0f : 0.0f)
        };
    }

    f32 flySpeedScale(const InputProvider& input, f32 scale) {
        constexpr f32 NotchFactor = 1.25f;
        constexpr f32 MinScale = 1.0f / 8.0f;
        constexpr f32 MaxScale = 8.0f;

        const auto notches = input.GetMouseWheel();
        if(notches == 0.0f)
            return scale;

        return std::clamp(
            scale * std::pow(NotchFactor, notches),
            MinScale,
            MaxScale
        );
    }

    Vec4 flyRotation(f32 yaw, f32 pitch) {
        return quat(rotateY(yaw), rotateX(pitch));
    }

    Vec3 flyMove(
        Vec3 position,
        Vec4 rotation,
        Vec3 moveInput,
        f32 speed,
        f64 seconds
    ) {
        const auto rot = rotateMat(rotation);
        const auto right = static_cast<Vec3>(rot[0]);
        const auto forward = static_cast<Vec3>(rot[2]);

        const auto direction = normalize(
            right * moveInput.x + unitY() * moveInput.y + forward * moveInput.z
        );

        const auto moveAmount = speed * static_cast<f32>(seconds);
        return position + direction * moveAmount;
    }

    Transform flyStep(
        const Transform& local,
        f32 yaw,
        f32 pitch,
        Vec3 moveInput,
        f32 speed,
        f64 seconds
    ) {
        const auto rotation = flyRotation(yaw, pitch);
        const auto position =
            moveInput == zeros()
                ? local.position
                : flyMove(local.position, rotation, moveInput, speed, seconds);

        return Transform{
            .position = position,
            .rotation = rotation,
            .scale = local.scale
        };
    }
}
