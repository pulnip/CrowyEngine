#pragma once

#include "Camera.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct EditorCut;

    // what a camera sees through, apart from where it stands
    struct EditorLens {
        // vertical, radians
        f32 fovY = 1.0f;
        f32 nearZ = 0.3f;
        f32 farZ = 1000.0f;
        bool orthographic = false;
        // half the vertical extent in meters, for an orthographic lens
        f32 orthoHalfHeight = 10.0f;
    };

    // a named pose and lens, in the engine's signs: yaw clockwise from +Z,
    // positive pitch looks down
    struct EditorCut {
        Str name;
        Vec3 position{};
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        EditorLens lens;
    };

    // A fly camera that snaps to cuts and can look through an orthographic
    // lens; held keys are ignored while keyboardGated.
    class EditorCamera final: public Camera {
    public:
        // public so the pose can be registered as reflected properties
        Vec3 position{};
        f32 yaw = 0.0f;
        f32 pitch = 0.0f;
        EditorLens lens;
        f32 moveSpeed = 15.0f;
        f32 lookSensitivity = 0.003f;
        // a text field has the keyboard
        bool keyboardGated = false;

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
        // unit, the way the camera looks
        Vec3 Forward() const noexcept;
        // whether input moved or turned the camera since the last call
        bool TakeMoved() noexcept;
    };
}
