#pragma once

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // Shared vocabulary of the two terrain samples.

    inline constexpr u32 TERRAIN_CELLS_X = 128;
    inline constexpr u32 TERRAIN_CELLS_Y = 64; // Y is height axis
    inline constexpr u32 TERRAIN_CELLS_Z = 128;

    // density is evaluated at cell corners
    inline constexpr u32 TERRAIN_CORNERS_X = TERRAIN_CELLS_X + 1;
    inline constexpr u32 TERRAIN_CORNERS_Y = TERRAIN_CELLS_Y + 1;
    inline constexpr u32 TERRAIN_CORNERS_Z = TERRAIN_CELLS_Z + 1;

    inline constexpr f32 TERRAIN_CELL_SIZE = 1.0f;

    // Mirrors TerrainParams in Engine/RHI/Sample/Terrain/TerrainDensity.slang.
    struct TerrainParams{
        u32 seed = 1337;
        // fbm2D base frequency, in cycles per world unit
        f32 freq = 0.02f;
        u32 octaves = 5;
        f32 heightBase = 24.0f;
        f32 heightAmp = 16.0f;
        f32 caveFreq = 0.06f;
        // cave noise is remapped to [0, 1]; only the excess over this carves
        f32 caveThreshold = 0.50f;
        // the carve has to out-run `baseHeight - y` - under ~60
        // nothing carves at all. 0.50 / 80 carves ~8% of the underground.
        f32 caveStrength = 80.0f;
    };
    static_assert(sizeof(TerrainParams) == 32);
    static_assert(alignof(TerrainParams) == 4);

    // Mirrors TerrainVertex in Engine/RHI/Sample/Terrain/Terrain.slang.
    // no UV (triplanar), no index buffer (triangle soup)
    struct TerrainVertex{
        Vec3 position;
        Vec3 normal;
    };
    static_assert(sizeof(TerrainVertex) == 24);

    // shared start pose, so the two samples can be compared side by side
    // (positive pitch looks down)
    inline constexpr Vec3 TERRAIN_CAMERA_START_POS{64.0f, 72.0f, -45.0f};
    inline constexpr f32 TERRAIN_CAMERA_START_YAW = 0.0f;
    inline constexpr f32 TERRAIN_CAMERA_START_PITCH = 0.42f;

    // Mirrors the `frame` cbuffer (b1) in Engine/RHI/Sample/Terrain/Terrain.slang.
    struct TerrainFrameUniforms{
        Mat4 viewProj;
        Vec3 toLight;
        f32 ambient;
    };
}
