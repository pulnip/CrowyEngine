#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class InputProvider;

    class Camera {
    public:
        CROWY_DECLARE_INTERFACE(Camera)

        virtual void ProcessInput(const InputProvider& input) {}
        virtual void Update(f64 deltaTime) {}

        virtual Vec3 Position() const noexcept = 0;
        virtual Mat4 View() const noexcept = 0;
        virtual Mat4 Projection(f32 aspect) const noexcept = 0;

        Mat4 ViewProj(f32 aspect) const noexcept {
            return Projection(aspect) * View();
        }
    };

    using CameraRAII = RAII<Camera>;
}
