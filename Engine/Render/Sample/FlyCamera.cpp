#include "FlyCamera.hpp"

#include <algorithm>
#include <numbers>

#include "ClassRegistry.hpp"
#include "InputProvider.hpp"
#include "Object.hpp"

namespace Crowy
{
    void FlyCamera::ProcessInput(const InputProvider& input) {
        constexpr auto pi_2 = std::numbers::pi_v<f32> / 2;

        if(input.IsKeyDown(MouseButton::RButton)) {
            const auto dpos = input.GetMouseDPos();

            yaw += dpos.x * config.lookSensitivity;
            pitch = std::clamp(
                pitch + dpos.y * config.lookSensitivity,
                -pi_2 + 0.01f,
                pi_2 - 0.01f
            );
            RecomputeView();
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
        RecomputeView();
    }

    Vec4 FlyCamera::Rotation() const noexcept {
        return quat(rotateY(yaw), rotateX(pitch));
    }

    Mat4 FlyCamera::Projection(f32 aspect) const noexcept {
        return perspective(config.fovY, aspect, config.nearZ, config.farZ);
    }

    void FlyCamera::RecomputeView() noexcept {
        view = viewMat(position, Rotation());
    }

    // the pose every sample exposes to its panel and the port
    // clang-format off
    CROWY_STRUCT(FlyCamera)
        .SetProperty("position", &FlyCamera::position)
        .SetProperty("yaw", &FlyCamera::yaw)
        .SetProperty("pitch", &FlyCamera::pitch)
        .SetUIRange(-1.55f, 1.55f)
        .SetProperty("fovY", &FlyCamera::config, &FlyCamera::Config::fovY)
        .SetUIRange(0.35f, 2.4f)
    CROWY_STRUCT_END(FlyCamera)
    // clang-format on
}
