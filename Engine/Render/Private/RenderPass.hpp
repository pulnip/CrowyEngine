#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"
#include "RHIPipelineState.hpp"
#include "RHISampler.hpp"
#include "RHIShader.hpp"

namespace Crowy
{
    struct CBuffer{
        using FieldName = CBufferFieldName;
        using FieldType = CBufferFieldType;
        using FieldOffset = CBufferFieldOffset;
        using FieldMeta = CBufferFieldMeta;

        std::string name;
        uint32_t slot;
        CBufferMeta meta;
        RHIBufferPtr buffer;
    };

    struct RenderPass{
        std::string name;
        bool enabled = true;

        // input Texture
        std::vector<std::string> inputs;
        // output RenderTarget
        std::vector<std::string> targets;
        std::string depthTarget;
        std::vector<RHISamplerPtr> fs_samplers;

        RHIShaderPtr vs, fs;
        std::optional<RenderTypeHash> renderType;
        RHIPipelineStatePtr pipeline;

        std::vector<CBuffer> fs_cbuffers;

        inline bool isFullscreenPass() const{
            return !renderType.has_value();
        }
    };
}