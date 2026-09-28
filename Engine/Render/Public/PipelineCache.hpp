#pragma once

#include <optional>
#include <span>
#include <unordered_map>

#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RHIPipelineState.hpp"
#include "RenderMaterial.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // set, each replaces the same-named state in every color pass of a frame;
    // the debug views are the first to set them
    struct MeshPassOverride {
        std::optional<RHIFillMode> fillMode;
        std::optional<RHIBlendState> blend;
        std::optional<RHIComparisonFunc> depthFunc;
        std::optional<bool> depthWrite;
    };

    // replaces the rasterizer bias of every material in the pass
    struct PassDepthBias {
        i32 depthBias = 0;
        f32 depthBiasClamp = 0.0f;
        f32 slopeScaledDepthBias = 0.0f;
    };

    // what a mesh pass's desc says about the pipelines it draws with
    struct MeshPassState {
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        bool depthWrite = true;
        // replaces the material's; a pass with no color target has none
        std::optional<RHIShaderDesc> fragmentShader;
        std::optional<PassDepthBias> depthBias;
    };

    // The half of a pipeline state a pass owns.
    struct PassPipelineDesc {
        // empty: a depth-only pass, which has no fragment stage
        std::span<const RHIPixelFormat> renderTargetFormats;
        RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
        MeshPassState state;
        // a depth-only pass ignores it
        MeshPassOverride debug;
    };

    RHIGraphicsPipelineStateDesc Compose(
        const MaterialPipelineDesc& material,
        const PassPipelineDesc& pass
    );

    using PipelineStates = std::unordered_map<
        RHIGraphicsPipelineStateDesc,
        RHIGraphicsPipelineStateRAII>;

    // what one Rebuild did
    struct PipelineRebuild {
        usize pipelines = 0;
        f64 milliseconds = 0.0;
    };

    class PipelineCache {
    private:
        RHIDevice& device;
        PipelineStates states;

    public:
        ~PipelineCache();
        CROWY_DECLARE_PINNED(PipelineCache)

        explicit PipelineCache(RHIDevice& device)
            : device(device) {}

        RHIGraphicsPipelineState& Resolve(
            const MaterialPipelineDesc& material,
            const PassPipelineDesc& pass
        );

        usize Count() const noexcept { return states.size(); }

        // recompiles every stored key into new states and swaps only when
        // all of them built; a failure leaves the old states live and
        // rethrows with the key's shaders named
        PipelineRebuild Rebuild();
    };
}
