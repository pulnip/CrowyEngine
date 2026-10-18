#include "FlyCamera.hpp"

#include "FlyControl.hpp"

namespace Crowy
{
    void FlyCamera::ProcessInput(const InputProvider& input) {
        if(flyLook(input, config.lookSensitivity, yaw, pitch))
            RecomputeView();

        moveInput = flyMoveInput(input);
        speedScale = flySpeedScale(input, speedScale);
    }

    void FlyCamera::Update(f64 deltaTime) {
        if(moveInput == zeros())
            return;

        position = flyMove(
            position,
            Rotation(),
            moveInput,
            config.moveSpeed * speedScale,
            deltaTime
        );
        RecomputeView();
    }

    Vec4 FlyCamera::Rotation() const noexcept {
        return flyRotation(yaw, pitch);
    }

    Mat4 FlyCamera::Projection(f32 aspect) const noexcept {
        return perspective(config.fovY, aspect, config.nearZ, config.farZ);
    }

    void FlyCamera::RecomputeView() noexcept {
        view = viewMat(position, Rotation());
    }
}
