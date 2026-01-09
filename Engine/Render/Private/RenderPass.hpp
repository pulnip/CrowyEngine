#pragma once

#include <cstdint>
#include <string>
#include <vector>
#include "ResourceHandle.hpp"
#include "RHIPipelineState.hpp"
#include "RHIShader.hpp"

namespace Crowy
{
    enum class ResourceUsage: uint8_t{
        Read,
        Write,
        ReadWrite
    };

    struct ResourceBinding{
        // "GBuffer_Albedo", "SceneDepth", etc...
        std::string name;
        ResourceUsage usage;
    };

    struct RenderPass{
        std::string name;
        RenderType renderType;
        RHIShaderPtr vs, fs;
        RHIPipelineStatePtr pipeline;

        std::vector<ResourceBinding> bindings;

        inline bool isFullscreenPass() const{
            return renderType.empty();
        }
    };
}