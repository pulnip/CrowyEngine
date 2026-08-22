#include "FlyCamera.hpp"

#include <algorithm>
#include <numbers>

#include "InputProvider.hpp"

namespace Crowy
{
    void FlyCamera::ProcessInput(const InputProvider& input) {
        if(input.IsKeyDown(MouseButton::RButton)) {
            const auto dpos = input.GetMouseDPos();

            yaw += dpos.x * config.lookSensitivity;
            pitch = std::clamp(
                pitch + dpos.y * config.lookSensitivity,
                -std::numbers::pi_v<f32> / 2 + 0.01f,
                std::numbers::pi_v<f32> / 2 - 0.01f
            );
        }

        moveInput = Vec3{
            (input.IsKeyDown(KeyCode::D) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::A) ? 1.0f : 0.0f),
            (input.IsKeyDown(KeyCode::Space) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::Shift) ? 1.0f : 0.0f),
            (input.IsKeyDown(KeyCode::W) ? 1.0f : 0.0f) -
                (input.IsKeyDown(KeyCode::S) ? 1.0f : 0.0f)
        };
    }

    void FlyCamera::Update(f64 deltaTime) {
        if(moveInput == zeros())
            return;

        const auto rot = rotateMat(Rotation());
        const auto right = static_cast<Vec3>(rot[0]);
        const auto forward = static_cast<Vec3>(rot[2]);

        const auto direction = normalize(
            right * moveInput.x + unitY() * moveInput.y + forward * moveInput.z
        );

        const auto moveAmount = config.moveSpeed * static_cast<f32>(deltaTime);
        position = position + direction * moveAmount;
    }

    Vec4 FlyCamera::Rotation() const noexcept {
        return quat(rotateY(yaw), rotateX(pitch));
    }

    Mat4 FlyCamera::View() const noexcept {
        return viewMat(position, Rotation());
    }

    Mat4 FlyCamera::Projection(f32 aspect) const noexcept {
        return perspective(config.fovY, aspect, config.nearZ, config.farZ);
    }
}
