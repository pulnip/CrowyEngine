#include "OrbitCamera.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "InputProvider.hpp"

namespace Crowy
{
    void OrbitCamera::ProcessInput(const InputProvider& input) {
        if(!input.IsKeyDown(MouseButton::RButton))
            return;

        const auto dpos = input.GetMouseDPos();
        constexpr f32 PitchLimit = 0.5f * std::numbers::pi_v<f32> - 0.01f;

        yaw -= dpos.x * config.lookSensitivity;
        pitch = std::clamp(
            pitch + dpos.y * config.lookSensitivity,
            -PitchLimit,
            PitchLimit
        );
    }

    Vec3 OrbitCamera::Position() const noexcept {
        const auto cosYaw = std::cos(yaw), sinYaw = std::sin(yaw);
        const auto cosPitch = std::cos(pitch), sinPitch = std::sin(pitch);

        return config.target +
               config.distance *
                   Vec3{cosPitch * sinYaw, sinPitch, -cosPitch * cosYaw};
    }

    Mat4 OrbitCamera::View() const noexcept {
        return lookAt(Position(), config.target, unitY());
    }

    Mat4 OrbitCamera::Projection(f32 aspect) const noexcept {
        return perspective(config.fovY, aspect, config.nearZ, config.farZ);
    }
}
