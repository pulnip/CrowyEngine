#pragma once

#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "TerrainUI.hpp"

namespace Crowy
{
    inline constexpr RHIPixelFormat TERRAIN_DEPTH_FORMAT = RHIPixelFormat::D32_FLOAT;

    // Mirrors ResourceData in Engine/RHI/Sample/Terrain/Terrain.slang.
    struct TerrainSurfacePush{
        u64 vertices;
    };
    static_assert(sizeof(TerrainSurfacePush) == 8);

    // The pipelines that draw Engine/RHI/Sample/Terrain/Terrain.slang, one per debug
    // toggle combination. Vertices are pulled by SV_VertexID from a structured
    // buffer, so there is no input layout and none of this depends on how the
    // mesh was built - which is why both samples can share it.
    //
    // Keeping the entry point names in one place matters more than the line
    // count: they are the contract with a shader file both samples load.
    class TerrainSurface{
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
