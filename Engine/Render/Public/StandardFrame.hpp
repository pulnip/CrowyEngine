#pragma once

#include <vector>

#include "FrameGraph.hpp"
#include "PostChain.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RenderMaterial.hpp"

namespace Crowy
{
    struct StandardFrameConfig;

    // the extent and the target a scene view draws into
    inline constexpr StrView SceneViewName = "SceneView";
    inline constexpr CStr StandardForwardShader =
        "Engine/Render/Shader/StandardForward.slang";

    FrameGraphDesc describeStandardFrame(const StandardFrameConfig& config);
    // StandardForwardShader's entries for one domain; Translucent blends by
    // the surface's alpha
    MaterialPipelineDesc standardMaterial(
        MaterialDomain domain,
        RHICullMode cull = RHICullMode::Back
    );

    struct StandardFrameConfig {
        RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
        // what the color passes write; RGBA8_UNORM for a scene already in
        // display values
        RHIPixelFormat sceneColorFormat = RHIPixelFormat::RGBA16_FLOAT;
        // the scene color's clear at creation; the walker clears with
        // FrameInputs::sceneClear
        Color clearColor = Colors::Black;
        // each mesh pass's list reserve
        u32 drawCapacity = 256;
        bool depthPrepass = true;
        // the directional shadow map's side
        u32 shadowMapSize = 2048;
        std::vector<PostPassDesc> post{tonemapPass()};
        bool sceneView = false;
        // the back buffer's, so the UI copies the picture as it would land
        // there
        RHIPixelFormat sceneViewFormat = RHIPixelFormat::RGBA8_UNORM;

        friend bool operator==(
            const StandardFrameConfig&,
            const StandardFrameConfig&
        ) = default;
    };
}
