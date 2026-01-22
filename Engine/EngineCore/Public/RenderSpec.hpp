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
        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        RHIBlendState blend = {};
    };

    struct RenderSpec{
        std::unordered_map<std::string, RHITextureCreateDesc> renderTargets;
        std::vector<RenderPassSpec> passes;
    };
}