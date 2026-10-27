#pragma once

#include "LinearAlgebra.hpp"
#include "Terrain.hpp"

namespace Crowy
{
    class InputProvider;

    class TerrainCamera {
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

        Vec3 Position() const noexcept { return position; }
        Vec4 Rotation() const;
        Mat4 ViewProj() const;
    };
}
