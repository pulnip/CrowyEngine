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

    struct BindSpec{
        std::string name;
        uint32_t slot;
    };

    struct PipelineBindSpec{
        // input Texture
        std::vector<std::string> inputs;
        // output Texture
        std::vector<std::string> outputs;
        // depth buffer
        std::string depthOutput;

        std::vector<BindSpec> fs_samplers;
        std::vector<BindSpec> fs_cbuffers;

        ShaderSpec shader;
        RHIRasterizerState rasterizer = {};
        std::optional<RHIDepthStencilState> depthStencil = std::nullopt;
        RHIBlendState blend = {};

        RenderType renderType;
    };

    struct RenderPassSpec{
        std::string name;
        std::vector<PipelineBindSpec> pipelines;
    };

    struct RenderSpec{
        std::unordered_map<std::string, RHITextureCreateDesc> textures;
        std::unordered_map<std::string, RHISamplerState> samplers;
        std::unordered_map<std::string, CBuffer> cbuffers;
        std::vector<RenderPassSpec> passes;
    };
}