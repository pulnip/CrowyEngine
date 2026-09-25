#pragma once

#include <span>
#include <unordered_map>

#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RHIPipelineState.hpp"
#include "RenderMaterial.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // The half of a pipeline state a pass owns.
    struct PassPipelineDesc {
        std::span<const RHIPixelFormat> renderTargetFormats;
        RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
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
