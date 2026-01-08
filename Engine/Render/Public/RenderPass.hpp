#pragma once

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>
#include "math.hpp"
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    enum class ResourceUsage: uint8_t{
        Read,
        Write,
        ReadWrite
    };

    struct RenderContext{
        std::span<const RenderItem> renderItems;
        // Camera Information
        Mat4 view, proj;
        RHIViewport viewport;
    };

    struct ResourceBinding{
        // "GBuffer_Albedo", "SceneDepth", etc...
        std::string name;
        ResourceUsage usage;
    };

    struct RenderPass{
        std::string name;
        ShaderHandle shader;

        RenderType renderType;

        std::vector<ResourceBinding> bindings;

        inline bool isFullscreenPass() const{
            return renderType.empty();
        }
    };
}