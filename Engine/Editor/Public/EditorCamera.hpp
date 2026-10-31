#pragma once

#include "Camera.hpp"
#include "Geometry/Projection.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // yaw clockwise from +Z, positive pitch looks down
    struct EditorCut {
        Str name;
        Vec3 position{};
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        CameraLens lens;

        bool operator==(const EditorCut&) const = default;
    };

    class EditorCamera final: public Camera {
    public:
        Vec3 position{};
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        CameraLens lens;
        f32 moveSpeed = 15.0f;
        f32 lookSensitivity = 0.003f;

    private:
        Vec3 moveInput{};
        bool fast = false;
        bool moved = false;
        Mat4 view = unitMat();

    public:
        explicit EditorCamera(const EditorCut& cut) noexcept;

        void ProcessInput(const InputProvider& input) override;
        void Update(f64 deltaTime) override;

        Vec3 Position() const noexcept override { return position; }
        Mat4 View() const noexcept override { return view; }
        Mat4 Projection(f32 aspect) const noexcept override;

        void Snap(const EditorCut& cut) noexcept;
        void RecomputeView() noexcept;
        Vec4 Rotation() const noexcept;
        Vec3 Forward() const noexcept;
        bool TakeMoved() noexcept;
    };
}
