#include "PipelineCache.hpp"

#include <algorithm>
#include <chrono>
#include <exception>
#include <format>
#include <stdexcept>
#include <utility>

#include "Assert.hpp"
#include "LogLocal.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "StringUtil.hpp"

namespace
{
    // a key's shaders, as a failed rebuild names them
    Crowy::Str describeShaders(const Crowy::RHIGraphicsPipelineStateDesc& desc) {
        using namespace Crowy;

        const auto& vertex =
            std::get<RHILegacyFrontendDesc>(desc.preRasterizer).vertexShader;
        if(!desc.fragmentShader) {
            return std::format(
                "{} ({})",
                toUTF8String(vertex.path),
                vertex.entryPoint
            );
        }

        const auto& fragment = *desc.fragmentShader;
        if(vertex.path == fragment.path) {
            return std::format(
                "{} ({}, {})",
                toUTF8String(vertex.path),
                vertex.entryPoint,
                fragment.entryPoint
            );
        }

        return std::format(
            "{} ({}), {} ({})",
            toUTF8String(vertex.path),
            vertex.entryPoint,
            toUTF8String(fragment.path),
            fragment.entryPoint
        );
    }
}

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
            // always present, even when the pass overrides it: without it
            // Metal loses the depth format and D3D12 gets DSVFormat UNKNOWN
            .depthStencil =
                RHIDepthStencilState{
                    .format = pass.depthFormat,
                    .depthWriteEnable =
                        pass.depthWrite.value_or(material.depthWrite),
                    .depthFunc = pass.depthFunc.value_or(material.depthFunc)
                },
            .blend = pass.blend ? pass.blend : material.blend,
            .renderTargetCount = pass.renderTargetFormats.size(),
            .profile = material.profile
        };
        if(pass.fillMode)
            desc.rasterizer.fillMode = *pass.fillMode;
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

    PipelineRebuild PipelineCache::Rebuild() {
        const auto started = std::chrono::steady_clock::now();

        PipelineStates rebuilt;
        rebuilt.reserve(states.size());
        for(const auto& [desc, _]: states) {
            try {
                rebuilt.emplace(desc, device.CreatePipelineState(desc));
            } catch(const std::exception& e) {
                throw std::runtime_error(
                    std::format("{}: {}", describeShaders(desc), e.what())
                );
            }
        }

        // one in-order queue: every list that bound an old state was
        // submitted before the next frame, which binds only new ones
        std::swap(states, rebuilt);
        for(auto& [_, old]: rebuilt)
            device.Retire(std::move(old));

        const std::chrono::duration<f64, std::milli> elapsed =
            std::chrono::steady_clock::now() - started;
        LOG_INFO(
            "reloaded {} pipelines in {:.0f} ms",
            states.size(),
            elapsed.count()
        );

        return PipelineRebuild{
            .pipelines = states.size(),
            .milliseconds = elapsed.count()
        };
    }
}
