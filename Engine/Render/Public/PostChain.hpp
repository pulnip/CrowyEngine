#pragma once

#include <span>
#include <vector>

#include "FrameGraph.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

// The fullscreen entries after the scene passes, as data: each reads the
// color the chain has so far, and the last one writes the output.
namespace Crowy
{
    struct PostPassDesc;

    // which side of the tone map an entry's output lies on
    enum class PostOutput : u8 {
        // linear radiance, in scene color's format
        Scene,
        // display values, RGBA8_UNORM
        Display,
    };

    // entry i reads the previous output, or scene color, and writes `output`
    // if last; a refused list throws and leaves desc as it was
    void appendPostChain(
        FrameGraphDesc& desc,
        FrameTargetID sceneColor,
        std::span<const PostPassDesc> post,
        FrameTargetID output = BackBufferTarget
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
        std::vector<Str> inputs;
        Vec4 params{};

        friend bool operator==(const PostPassDesc&, const PostPassDesc&) =
            default;
    };
}
