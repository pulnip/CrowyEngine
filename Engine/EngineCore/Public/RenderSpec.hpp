#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "path_util.hpp"
#include "RenderDefinitions.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct ShaderSpec{
        std::filesystem::path vsFilePath;
        std::string vsFuncName;
        std::filesystem::path fsFilePath;
        std::string fsFuncName;
    };

    struct RenderPassSpec{
        std::string name;
        // input Texture
        std::vector<std::string> inputs;
        // output RenderTarget
        std::vector<std::string> targets;
        // depth buffer
        std::string depthTarget;

        ShaderSpec shader;
        RenderType renderType;

        RHIRasterizerState rasterizer = {};
        RHIDepthStencilState depthStencil = {};
        RHIBlendState blend = {};
    };

    struct RenderTargetSpec{
        std::string name;
        // 0 for same as screen
        RHITextureCreateDesc desc;

        inline bool isScreenRelative() const{
            return desc.width == 0 || desc.height == 0;
        }
    };

    struct RenderSpec{
        std::unordered_map<std::string, RenderTargetSpec> renderTargets;
        std::vector<RenderPassSpec> passes;
    };
}