#include "FlyCamera.hpp"

namespace Crowy
{
    void FlyCamera::ProcessInput(const InputProvider& input) {
        if(fly.ProcessInput(input, config.lookSensitivity))
            RecomputeView();
    }

    void FlyCamera::Update(f64 deltaTime) {
        if(!fly.Moving())
            return;

        const auto moved = fly.Step(
            Transform{.position = position},
            config.moveSpeed,
            deltaTime
        );
        position = moved.position;
        RecomputeView();
    }

    Vec4 FlyCamera::Rotation() const noexcept {
        return flyRotation(fly.look.yaw, fly.look.pitch);
    }

    Mat4 FlyCamera::Projection(f32 aspect) const noexcept {
        return perspective(config.fovY, aspect, config.nearZ, config.farZ);
    }

    void FlyCamera::RecomputeView() noexcept {
        view = viewMat(position, Rotation());
    }
}
