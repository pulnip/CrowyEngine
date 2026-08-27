#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "TerrainSurface.hpp"

namespace{
    using namespace Crowy;

    constexpr CStr SHADER_PATH = "Engine/RHI/Sample/Terrain/Terrain.slang";

    RHIGraphicsPipelineStateDesc PipelineDesc(
        RHIPixelFormat colorFormat,
        RHIPixelFormat depthFormat,
        RHIFillMode fillMode,
        bool debugNormal
    ){
        return RHIGraphicsPipelineStateDesc{
            // no vertex layout: vertices are pulled by SV_VertexID
            .preRasterizer = RHILegacyFrontendDesc{
                .topology = RHIPrimitiveTopology::TriangleList,
                .vertexShader = {.path = SHADER_PATH, .entryPoint = "vs_main"}
            },
            .rasterizer = RHIRasterizerState{
                .fillMode = fillMode,
                // left-handed, so the right-hand normal is the front face
                .frontCounterClockwise = false
            },
            .fragmentShader = {
                .path = SHADER_PATH,
                .entryPoint = debugNormal ? "fs_debug_normal" : "fs_main"
            },
            .depthStencil = RHIDepthStencilState{
                .format = depthFormat,
                .depthWriteEnable = true,
                .depthFunc = RHIComparisonFunc::Less
            },
            .renderTargetFormats = {colorFormat},
            .renderTargetCount = 1
        };
    }
}

namespace Crowy
{
    TerrainSurface::TerrainSurface(
        RHIDevice& device,
        RHIPixelFormat colorFormat,
        RHIPixelFormat depthFormat
    )
        : fillShaded(device.CreatePipelineState(
            PipelineDesc(colorFormat, depthFormat, RHIFillMode::Solid, false)
        ))
        , wireShaded(device.CreatePipelineState(
            PipelineDesc(colorFormat, depthFormat, RHIFillMode::Wireframe, false)
        ))
        , fillNormal(device.CreatePipelineState(
            PipelineDesc(colorFormat, depthFormat, RHIFillMode::Solid, true)
        ))
        , wireNormal(device.CreatePipelineState(
            PipelineDesc(colorFormat, depthFormat, RHIFillMode::Wireframe, true)
        ))
    {}

    TerrainSurface::~TerrainSurface() = default;

    RHIGraphicsPipelineState& TerrainSurface::Pipeline(const TerrainDebug& debug) const{
        if(debug.showNormals)
            return debug.wireframe ? *wireNormal : *fillNormal;

        return debug.wireframe ? *wireShaded : *fillShaded;
    }
}
