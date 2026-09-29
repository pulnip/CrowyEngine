#pragma once

#include <vector>

#include "FramePipeline.hpp"
#include "PostChain.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

// The leaf that is only data: the pass list the engine draws untouched.
namespace Crowy
{
    struct StandardPipelineConfig;

    FramePipelineDesc makeStandardPipeline(
        const StandardPipelineConfig& config
    );

    struct StandardPipelineConfig {
        RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
        // what the colour passes write; RGBA8_UNORM for a scene already in
        // display values
        RHIPixelFormat sceneColorFormat = RHIPixelFormat::RGBA16_FLOAT;
        // the scene colour's clear, at creation and in the pass alike
        Color clearColor = Colors::Black;
        // each mesh pass's list reserve
        u32 drawCapacity = 256;
        // off, Opaque tests Less and writes depth itself
        bool depthPrepass = true;
        // the directional shadow map's side; 0 is no map and no Shadow pass
        u32 shadowMapSize = 2048;
        // the entries after the scene passes; the last one writes the back
        // buffer and carries the UI
        std::vector<PostPassDesc> post{tonemapPass()};

        friend bool operator==(
            const StandardPipelineConfig&,
            const StandardPipelineConfig&
        ) = default;
    };
}
