#include <algorithm>
#include <numbers>
#include "InputProvider.hpp"
#include "TerrainCamera.hpp"

#include <imgui.h>

namespace{
    using namespace Crowy;

    constexpr f32 FOV_Y = std::numbers::pi_v<f32> / 3;
    constexpr f32 NEAR_Z = 0.1f, FAR_Z = 500.0f;
    // world units per second; the grid is 128 across, so this crosses it in
    // about three seconds
    constexpr f32 MOVE_SPEED = 40.0f;
    constexpr f32 LOOK_SENSITIVITY = 0.003f;

    constexpr f32 PITCH_LIMIT = std::numbers::pi_v<f32> / 2 - 0.01f;
}

namespace Crowy
{
    void TerrainCamera::ProcessInput(const InputProvider& input){
        const auto& io = ImGui::GetIO();

        if(input.IsKeyDown(MouseButton::RButton) && !io.WantCaptureMouse){
            const auto dpos = input.GetMouseDPos();
            yaw += dpos.x * LOOK_SENSITIVITY;
            // straight up would leave the yaw axis undefined
            pitch = std::clamp(
                pitch + dpos.y * LOOK_SENSITIVITY,
                -PITCH_LIMIT,
                PITCH_LIMIT
            );
        }

        if(io.WantCaptureKeyboard){
            moveInput = Vec3{};
            return;
        }

        moveInput = Vec3{
            (input.IsKeyDown(KeyCode::D) ? 1.0f : 0.0f) -
            (input.IsKeyDown(KeyCode::A) ? 1.0f : 0.0f),
            (input.IsKeyDown(KeyCode::Space) ? 1.0f : 0.0f) -
            (input.IsKeyDown(KeyCode::Shift) ? 1.0f : 0.0f),
            (input.IsKeyDown(KeyCode::W) ? 1.0f : 0.0f) -
            (input.IsKeyDown(KeyCode::S) ? 1.0f : 0.0f)
        };
    }

    void TerrainCamera::Update(f64 deltaTime){
        if(moveInput.x == 0.0f && moveInput.y == 0.0f && moveInput.z == 0.0f)
            return;

        const auto rot = rotateMat(Rotation());
        const auto right = static_cast<Vec3>(rot[0]);
        const auto forward = static_cast<Vec3>(rot[2]);

        // up stays world up, so looking down does not sink the climb
        const auto direction = normalize(
            right * moveInput.x +
            unitY() * moveInput.y +
            forward * moveInput.z
        );
        position += direction * (MOVE_SPEED * static_cast<f32>(deltaTime));
    }

    void TerrainCamera::SetViewport(u32 width, u32 height){
        aspect = static_cast<f32>(width) / height;
    }

    Vec4 TerrainCamera::Rotation() const{
        return quat(rotateY(yaw), rotateX(pitch));
    }

    Mat4 TerrainCamera::ViewProj() const{
        return perspective(FOV_Y, aspect, NEAR_Z, FAR_Z) *
            viewMat(position, Rotation());
    }
}
