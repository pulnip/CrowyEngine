#pragma once

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
        // each mesh pass's list reserve
        u32 drawCapacity = 256;
        // off, Opaque tests Less and writes depth itself
        bool depthPrepass = true;
        // the directional shadow map's side; 0 is no map and no Shadow pass
        u32 shadowMapSize = 2048;
    };
}
