#pragma once

#include <span>
#include <vector>

#include "FramePipeline.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

// The fullscreen entries after the scene passes, as data: each reads the
// colour the chain has so far, and the last one writes the back buffer.
namespace Crowy
{
    struct PostPassDesc;

    // which side of the tone map an entry's output lies on
    enum class PostOutput : u8 {
        // linear radiance, in scene colour's format
        Scene,
        // display values, RGBA8_UNORM
        Display,
    };

    // One fullscreen pass per entry: entry i reads the previous output
    // (scene colour for the first) and its inputs, and writes the back
    // buffer if last, else a lazily added intermediate of its class that is
    // not its source. Throws std::invalid_argument naming the entry and the
    // rule a list breaks, leaving desc as it was.
    void appendPostChain(
        FramePipelineDesc& desc,
        FrameTargetID sceneColor,
        std::span<const PostPassDesc> post
    );

    // Engine/Render/Shader/Tonemap.slang: linear radiance in, display out
    PostPassDesc tonemapPass();
    // Engine/Render/Shader/Present.slang: an exact copy, for a scene already
    // in display values
    PostPassDesc presentPass();

    struct PostPassDesc {
        Str name;
        // the VS is Fullscreen.slang's
        RHIShaderDesc fragmentShader;
        PostOutput output = PostOutput::Display;
        // further named targets, bound in order after the source
        std::vector<Str> inputs;
        Vec4 params{};
    };
}
