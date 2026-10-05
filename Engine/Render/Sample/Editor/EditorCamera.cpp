#include "EditorCamera.hpp"

#include <algorithm>
#include <numbers>
#include <utility>

#include "InputProvider.hpp"

namespace Crowy
{
    EditorCamera::EditorCamera(const EditorCut& cut) noexcept {
        Snap(cut);
    }

    void EditorCamera::ProcessInput(const InputProvider& input) {
        constexpr auto HalfPi = std::numbers::pi_v<f32> / 2;

        if(input.IsKeyDown(MouseButton::RButton)) {
            const auto delta = input.GetMouseDPos();
            if(delta.x != 0.0f || delta.y != 0.0f) {
                yaw += delta.x * lookSensitivity;
                pitch = std::clamp(pitch + delta.y * lookSensitivity, -HalfPi + 0.01f, HalfPi - 0.01f);
                moved = true;
                RecomputeView();
            }
        }

        if(keyboardGated) {
            moveInput = zeros();
            return;
        }
        const auto axis = [&](KeyCode positive, KeyCode negative) {
            return (input.IsKeyDown(positive) ? 1.0f : 0.0f) - (input.IsKeyDown(negative) ? 1.0f : 0.0f);
        };
        moveInput = Vec3{axis(KeyCode::D, KeyCode::A), axis(KeyCode::E, KeyCode::Q), axis(KeyCode::W, KeyCode::S)};
        fast = input.IsKeyDown(KeyCode::Shift);
    }

    void EditorCamera::Update(f64 deltaTime) {
        if(moveInput == zeros())
            return;

        const auto rotation = rotateMat(Rotation());
        const auto right = static_cast<Vec3>(rotation[0]);
        const auto forward = static_cast<Vec3>(rotation[2]);
        const auto direction =
            normalize(right * moveInput.x + unitY() * moveInput.y + forward * moveInput.z);
        const auto speed = moveSpeed * (fast ? 4.0f : 1.0f);

        position = position + direction * (speed * static_cast<f32>(deltaTime));
        moved = true;
        RecomputeView();
    }

    Mat4 EditorCamera::Projection(f32 aspect) const noexcept {
        if(lens.orthographic) {
            const auto height = 2.0f * lens.orthoHalfHeight;
            return orthographic(height * aspect, height, lens.nearZ, lens.farZ);
        }

        return perspective(lens.fovY, aspect, lens.nearZ, lens.farZ);
    }

    void EditorCamera::Snap(const EditorCut& cut) noexcept {
        position = cut.position;
        yaw = cut.yaw;
        pitch = cut.pitch;
        lens = cut.lens;
        moveInput = zeros();
        RecomputeView();
    }

    void EditorCamera::RecomputeView() noexcept {
        view = viewMat(position, Rotation());
    }

    Vec4 EditorCamera::Rotation() const noexcept {
        return quat(rotateY(yaw), rotateX(pitch));
    }

    Vec3 EditorCamera::Forward() const noexcept {
        return static_cast<Vec3>(rotateMat(Rotation())[2]);
    }

    bool EditorCamera::TakeMoved() noexcept {
        return std::exchange(moved, false);
    }
}
