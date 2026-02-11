#pragma once

#include <optional>
#include <string>
#include <vector>
#include "CBuffer.hpp"
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"
#include "RHIPipelineState.hpp"
#include "RHISampler.hpp"
#include "RHIShader.hpp"

namespace Crowy
{
    struct RenderPass{
        std::string name;
        bool enabled = true;

        // input Texture
        std::vector<std::string> inputs;
        // output RenderTarget
        std::vector<std::string> targets;
        std::string depthTarget;
        std::vector<RHISamplerPtr> fs_samplers;

        RHIShaderPtr vs, fs;
        std::optional<RenderTypeHash> renderType;
        RHIPipelineStatePtr pipeline;

        std::vector<CBuffer> fs_cbuffers;

        inline bool isFullscreenPass() const{
            return !renderType.has_value();
        }
    };
}