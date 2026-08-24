#include "PipelineCache.hpp"

#include <algorithm>

#include "Assert.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    RHIGraphicsPipelineStateDesc Compose(
        const MaterialPipelineDesc& material,
        const PassPipelineDesc& pass
    ) {
        CROWY_ASSERT(pass.renderTargetFormats.size() <= RHI_MAX_RENDER_TARGETS);

        RHIGraphicsPipelineStateDesc desc{
            .preRasterizer =
                RHILegacyFrontendDesc{
                    // no vertex layout: vertices are pulled by SV_VertexID, so
                    // a mesh's attribute set never reaches the pipeline key
                    .topology = material.topology,
                    .vertexShader = material.vertexShader
                },
            .rasterizer = material.rasterizer,
            .fragmentShader = material.fragmentShader,
            .depthStencil =
                RHIDepthStencilState{
                    .format = pass.depthFormat,
                    .depthWriteEnable = material.depthWrite,
                    .depthFunc = material.depthFunc
                },
            .blend = material.blend,
            .renderTargetCount = pass.renderTargetFormats.size(),
            .profile = material.profile
        };
        std::ranges::copy(
            pass.renderTargetFormats,
            desc.renderTargetFormats.begin()
        );

        return desc;
    }

    PipelineCache::~PipelineCache() = default;

    RHIGraphicsPipelineState& PipelineCache::Resolve(
        const MaterialPipelineDesc& material,
        const PassPipelineDesc& pass
    ) {
        auto desc = Compose(material, pass);

        const auto found = states.find(desc);
        if(found != states.end())
            return *found->second;

        auto state = device.CreatePipelineState(desc);
        const auto [inserted, _] =
            states.emplace(std::move(desc), std::move(state));

        return *inserted->second;
    }
}
