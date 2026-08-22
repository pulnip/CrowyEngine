#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    class InputProvider;

    class FlyCamera {
    public:
        // start pose, for frame capture comparable and reproducible
        struct Config {
            Vec3 position{};
            f32 yaw = 0.0f, pitch = 0.0f;

            f32 fovY = 1.0471976f;
            f32 nearZ = 0.1f, farZ = 1000.0f;

            f32 moveSpeed = 15.0f;
            f32 lookSensitivity = 0.003f;
        };

    private:
        Config config;
        Vec3 position{};
        f32 yaw = 0.0f, pitch = 0.0f;
        Vec3 moveInput{};

    public:
        explicit FlyCamera(const Config& config) noexcept
            : config(config),
              position(config.position),
              yaw(config.yaw),
              pitch(config.pitch) {}

        void ProcessInput(const InputProvider& input);
        void Update(f64 deltaTime);

        Vec3 Position() const noexcept { return position; }
        Vec4 Rotation() const noexcept;
        Mat4 View() const noexcept;
        Mat4 Projection(f32 aspect) const noexcept;

        Mat4 ViewProj(f32 aspect) const noexcept {
            return Projection(aspect) * View();
        }
    };
}
