#pragma once

#include "Camera.hpp"
#include "FlyControl.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    class FlyCamera: public Camera {
    public:
        struct Config {
            Vec3 position{};
            f32 yaw = 0.0f, pitch = 0.0f;

            f32 fovY = 1.0471976f;
            f32 nearZ = 0.1f, farZ = 1000.0f;

            f32 moveSpeed = 15.0f;
            f32 lookSensitivity = 0.003f;
        };

        // public so a sample can register the pose as reflected properties
    public:
        Config config;
        Vec3 position{};
        FlyState fly;

    private:
        Mat4 view = unitMat();

    public:
        explicit FlyCamera(const Config& config) noexcept
            : config(config),
              position(config.position),
              fly(CameraLook{.yaw = config.yaw, .pitch = config.pitch}) {
            RecomputeView();
        }

        void ProcessInput(const InputProvider& input) override;
        void Update(f64 deltaTime) override;

        Vec3 Position() const noexcept override { return position; }
        Vec4 Rotation() const noexcept;
        Mat4 View() const noexcept override { return view; }
        Mat4 Projection(f32 aspect) const noexcept override;

        void RecomputeView() noexcept;
    };
}
