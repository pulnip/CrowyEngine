#pragma once

#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "TerrainUI.hpp"

namespace Crowy
{
    inline constexpr RHIPixelFormat TERRAIN_DEPTH_FORMAT =
        RHIPixelFormat::D32_FLOAT;

    // Mirrors ResourceData in Terrain.slang.
    struct TerrainSurfacePush {
        u64 vertices;
    };
    static_assert(sizeof(TerrainSurfacePush) == 8);

    class TerrainSurface {
    private:
        RHIGraphicsPipelineStateRAII fillShaded, wireShaded;
        RHIGraphicsPipelineStateRAII fillNormal, wireNormal;

    public:
        TerrainSurface(
            RHIDevice&,
            RHIPixelFormat colorFormat,
            RHIPixelFormat depthFormat = TERRAIN_DEPTH_FORMAT
        );
        // the RAII members hold a type this header only forward-declares
        ~TerrainSurface();

        RHIGraphicsPipelineState& Pipeline(const TerrainDebug&) const;
    };
}
