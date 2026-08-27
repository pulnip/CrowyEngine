#pragma once

#include <algorithm>
#include "Terrain.hpp"
#include "Noise.hpp"

namespace Crowy
{
    namespace detail{
        // spelled out to match the Slang side expression exactly
        inline constexpr f32 terrainSmoothstep(f32 edge0, f32 edge1, f32 x) noexcept{
            const f32 t = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);

            return t * t * (3.0f - 2.0f * t);
        }

        // fades at both ends so caves never breach the floor or the surface
        inline constexpr f32 CaveMask(f32 y, f32 baseHeight) noexcept{
            // caves fade out within this distance of the floor and the surface
            constexpr f32 TERRAIN_CAVE_FADE = 4.0f;

            const f32 bedrockMask = terrainSmoothstep(
                0.0f,
                TERRAIN_CAVE_FADE,
                y
            );

            const f32 crustBottom = baseHeight - TERRAIN_CAVE_FADE;
            const f32 caveCeiling = crustBottom - TERRAIN_CAVE_FADE;
            const f32 surfaceMask = 1.0f - terrainSmoothstep(
                caveCeiling,
                crustBottom,
                y
            );

            return bedrockMask * surfaceMask;
        }
    }

    // CPU half of Engine/RHI/Sample/Terrain/TerrainDensity.slang.
    // density > 0 is solid; the isosurface is density == 0.

    inline f32 terrainBaseHeight(f32 x, f32 z, const TerrainParams& params) noexcept{
        return params.heightBase + params.heightAmp * fbm(
            x * params.freq,
            z * params.freq,
            params.seed,
            params.octaves
        );
    }

    inline f32 terrainDensity(
        Vec3 position,
        f32 baseHeight,
        const TerrainParams& params
    ) noexcept{
        // decorrelates the cave field from the surface fbm
        constexpr u32 CAVE_SEED_MIX = 0x5f356495u;
        const f32 solid = baseHeight - position.y;

        // remapped to [0, 1] so caveThreshold reads as a fraction
        const f32 cave = 0.5f + 0.5f * perlin(
            position.x * params.caveFreq,
            position.y * params.caveFreq,
            position.z * params.caveFreq,
            params.seed ^ CAVE_SEED_MIX
        );

        const f32 carve = std::max(0.0f, cave - params.caveThreshold) *
            params.caveStrength * detail::CaveMask(position.y, baseHeight);

        return solid - carve;
    }

    inline f32 terrainDensity(Vec3 position, const TerrainParams& params) noexcept{
        auto baseHeight = terrainBaseHeight(
            position.x,
            position.z,
            params
        );

        return terrainDensity(
            position,
            baseHeight,
            params
        );
    }
}
