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

    struct PassDepthBias {
        i32 depthBias = 0;
        f32 depthBiasClamp = 0.0f;
        f32 slopeScaledDepthBias = 0.0f;
    };

    // what a mesh pass's desc says about the pipelines it draws with
    struct MeshPassState {
        RHIComparisonFunc depthFunc = RHIComparisonFunc::Less;
        bool depthWrite = true;
        std::optional<RHIShaderDesc> fragmentShader;
        // false for a fragment stage that never calls the shading model, so
        // every material's model shares its pipeline
        bool linksShading = true;
        std::optional<PassDepthBias> depthBias;
    };

    // The half of a pipeline state a pass owns.
    struct PassPipelineDesc {
        // empty: a depth-only pass, whose only fragment stage is a Masked
        // material's cut
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
        // a desc no material and pass composed, as a fullscreen pass's
        RHIGraphicsPipelineState& Resolve(
            const RHIGraphicsPipelineStateDesc& desc
        );

        usize Count() const noexcept { return states.size(); }

        PipelineRebuild Rebuild();
    };
}
