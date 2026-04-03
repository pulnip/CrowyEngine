#pragma once

#include <filesystem>
#include <limits>
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

    struct BindSpec{
        std::string name;
        uint32_t slot = std::numeric_limits<uint32_t>::max();
    };

    struct ShaderSpec{
        std::filesystem::path vsFilePath;
        std::string vsFuncName;
        std::filesystem::path fsFilePath;
        std::string fsFuncName;

        inline auto hash() const{
            auto concat = to_utf8String(vsFilePath) + ':' + vsFuncName + ',' +
                          to_utf8String(fsFilePath) + ':' + fsFuncName;

            return std::hash<std::string>{}(concat);
        }
    };

    struct GraphicsPipelineBindSpec{
        // input Texture
        std::vector<std::string> inputs;

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

        // output Texture
        std::vector<std::string> outputs;
        // depth buffer
        std::string depthOutput;

        std::vector<GraphicsPipelineBindSpec> pipelines;
    };

    struct ComputeShaderSpec{
        std::filesystem::path filePath;
        std::string funcName;

        inline auto hash() const{
            auto concat = to_utf8String(filePath) + ':' + funcName;

            return std::hash<std::string>{}(concat);
        }
    };

    struct ComputePassSpec{
        std::string name;

        // input
        std::vector<BindSpec> inputTextures;
        std::vector<BindSpec> inputBuffers;

        // output
        std::vector<BindSpec> outputTextures;
        std::vector<BindSpec> outputBuffers;

        ComputeShaderSpec shader;
        RHISize3D gridSize;
        std::optional<RHISize3D> threadGroupSize = std::nullopt;
    };

    struct RenderSpec{
        std::unordered_map<std::string, RHITextureCreateDesc> textures;
        std::unordered_map<std::string, RHISamplerState> samplers;
        std::unordered_map<std::string, CBuffer> cbuffers;
        std::unordered_map<std::string, RHIBufferCreateDesc> buffers;

        std::vector<RenderPassSpec> renderPasses;
        std::vector<ComputePassSpec> computePasses;
    };
}