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
    };
}
