#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "path_util.hpp"
#include "RenderDefinitions.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct MaterialSpec{
        std::string baseColor;
        std::string targetSlot;
    };
    struct RenderObjectSpec{
        std::string uri;
        std::vector<MaterialSpec> material_override;
        std::string renderType;
    };

    struct ShaderSpec{
        using Key = std::string;
        using KeyHash = std::hash<Key>;

        std::filesystem::path vsFilePath;
        std::string vsFuncName;
        std::filesystem::path fsFilePath;
        std::string fsFuncName;

        inline Key key() const{
            return to_utf8String(vsFilePath) + ':' + vsFuncName + ',' +
                   to_utf8String(fsFilePath) + ':' + fsFuncName;
        }
    };

    struct RenderPassSpec{
        std::string name;

        // input Texture
        std::vector<std::string> inputs;
        // output RenderTarget
        std::vector<std::string> targets;
        // depth buffer
        std::string depthTarget;
        std::vector<RHISamplerState> fs_samplers;

        ShaderSpec shader;
        RenderType renderType;
        RHIRasterizerState rasterizer = {};
        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        RHIBlendState blend = {};

        std::vector<CBuffer> fs_cbuffers;
    };

    struct RenderSpec{
        std::unordered_map<std::string, RHITextureCreateDesc> renderTargets;
        std::vector<RenderPassSpec> passes;
    };
}