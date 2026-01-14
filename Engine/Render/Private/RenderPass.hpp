#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"
#include "RHIPipelineState.hpp"
#include "RHIShader.hpp"

namespace Crowy
{
    struct RenderPass{
        std::string name;
        bool enabled = true;
        std::optional<RenderTypeHash> renderType;
        RHIShaderPtr vs, fs;
        RHIPipelineStatePtr pipeline;

        // input Texture
        std::vector<std::string> inputs;
        // output RenderTarget
        std::vector<std::string> targets;
        std::string depthTarget;

        inline bool isFullscreenPass() const{
            return !renderType.has_value();
        }
    };
}