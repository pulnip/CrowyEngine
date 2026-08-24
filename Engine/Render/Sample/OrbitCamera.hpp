#pragma once

#include "Camera.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // Looks at a fixed target from a fixed distance; drag to swing around it.
    class OrbitCamera: public Camera {
    public:
        struct Config {
            Vec3 target{};
            f32 distance = 4.0f;
            f32 yaw = 0.0f, pitch = 0.0f;

            f32 fovY = 1.0471976f;
            f32 nearZ = 0.05f, farZ = 100.0f;

            f32 lookSensitivity = 0.005f;
        };

    private:
        Config config;
        f32 yaw = 0.0f, pitch = 0.0f;

    public:
        explicit OrbitCamera(const Config& config) noexcept
            : config(config), yaw(config.yaw), pitch(config.pitch) {}

        void ProcessInput(const InputProvider& input) override;

        Vec3 Position() const noexcept override;
        Mat4 View() const noexcept override;
        Mat4 Projection(f32 aspect) const noexcept override;
    };
}
