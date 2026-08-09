#pragma once

#include "LinearAlgebra.hpp"
#include "Terrain.hpp"

namespace Crowy
{
    class InputProvider;

    // The fly camera both terrain samples look through: hold RButton to look,
    // WASD to move, Space and Shift for up and down.
    //
    // Shared rather than copied because comparing the two samples is the whole
    // point - they have to frame the same field from the same place, and a
    // camera that drifted between them would quietly undo that.
    class TerrainCamera{
    private:
        Vec3 position = TERRAIN_CAMERA_START_POS;
        f32 yaw = TERRAIN_CAMERA_START_YAW;
        f32 pitch = TERRAIN_CAMERA_START_PITCH;

        Vec3 moveInput{};
        f32 aspect = 1.0f;

    public:
        // Input the UI is claiming is left alone, so typing a seed into the
        // panel does not also fly the camera away.
        void ProcessInput(const InputProvider&);
        void Update(f64 deltaTime);
        void SetViewport(u32 width, u32 height);

        Vec3 Position() const noexcept{ return position; }
        Vec4 Rotation() const;
        Mat4 ViewProj() const;
    };
}
