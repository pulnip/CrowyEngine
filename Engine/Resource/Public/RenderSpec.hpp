#pragma once

#include <string>
#include <vector>
#include "RenderDefinitions.hpp"

namespace Crowy
{
    struct ShaderSpec{
        std::string vsFilePath;
        std::string vsFuncName;
        std::string fsFilePath;
        std::string fsFuncName;
    };

    struct ResourceDependency{
        std::string name;
        bool isInput = false;
    };

    struct RenderPassSpec{
        std::string name;
        // output RenderTarget
        std::vector<std::string> targets;
        // input Texture
        std::vector<std::string> inputs;

        ShaderSpec shader;

        RenderType renderType;

        // DepthSpec depth;
        // BlendSpec blend;
        // RasterizerSpec rasterizer;
    };

    struct RenderTargetSpec{
        std::string name;
        // 0 for same as screen
        uint32_t width = 0, height = 0;
        // "RGBA8", "RGBA16F", "Depth24Stencil8", ...
        std::string format;

        inline bool isScreenRelative() const{
            return width == 0 || height == 0;
        }
    };

    struct RenderSpec{
        std::vector<RenderTargetSpec> renderTargets;
        std::vector<RenderPassSpec> passes;
    };
}