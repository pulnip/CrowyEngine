#pragma once

#include <cstdint>
#include <functional>
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
        RenderQueue queue;

        // Camera Information
        Mat4 view, proj;

        float deltaTime = 0.0f;
        uint64_t currentFrame = 0;
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