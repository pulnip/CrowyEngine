#pragma once

#include "Camera.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct EditorCut;

    inline constexpr bool sameCut(
        const EditorCut& a,
        const EditorCut& b
    ) noexcept;

    struct EditorLens {
        // vertical, radians
        f32 fovY = 1.0f;
        f32 nearZ = 0.3f;
        f32 farZ = 1000.0f;
        bool orthographic = false;
        // half the vertical extent in meters, for an orthographic lens
        f32 orthoHalfHeight = 10.0f;
    };

    // yaw clockwise from +Z, positive pitch looks down
    struct EditorCut {
        Str name;
        Vec3 position{};
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        EditorLens lens;
    };

    inline constexpr bool sameCut(
        const EditorCut& a,
        const EditorCut& b
    ) noexcept {
        const auto& x = a.lens;
        const auto& y = b.lens;

        return a.name == b.name && a.position == b.position && a.yaw == b.yaw &&
               a.pitch == b.pitch && x.fovY == y.fovY && x.nearZ == y.nearZ &&
               x.farZ == y.farZ && x.orthographic == y.orthographic &&
               x.orthoHalfHeight == y.orthoHalfHeight;
    }

    class EditorCamera final: public Camera {
    public:
        Vec3 position{};
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        EditorLens lens;
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
