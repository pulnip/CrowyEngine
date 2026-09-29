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
    Crowy::Str describeShaders(
        const Crowy::RHIGraphicsPipelineStateDesc& desc
    ) {
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
        auto shaders = vertex.path == fragment.path
                           ? std::format(
                                 "{} ({}, {})",
                                 toUTF8String(vertex.path),
                                 vertex.entryPoint,
                                 fragment.entryPoint
                             )
                           : std::format(
                                 "{} ({}), {} ({})",
                                 toUTF8String(vertex.path),
                                 vertex.entryPoint,
                                 toUTF8String(fragment.path),
                                 fragment.entryPoint
                             );
        for(const auto& module: desc.linkedModules)
            shaders += " + " + toUTF8String(module);

        return shaders;
    }
}

namespace Crowy
{
    RHIGraphicsPipelineStateDesc Compose(
        const MaterialPipelineDesc& material,
        const PassPipelineDesc& pass
    ) {
        CROWY_ASSERT(pass.renderTargetFormats.size() <= RHI_MAX_RENDER_TARGETS);

        const bool depthOnly = pass.renderTargetFormats.empty();
        const auto& state = pass.state;
        CROWY_ASSERT(
            !depthOnly || !state.fragmentShader,
            "a depth-only pass has no fragment stage to replace"
        );

        RHIGraphicsPipelineStateDesc desc{
            .preRasterizer =
                RHILegacyFrontendDesc{
                    // no vertex layout: vertices are pulled by SV_VertexID, so
                    // a mesh's attribute set never reaches the pipeline key
                    .topology = material.topology,
                    .vertexShader = material.vertexShader
                },
            .rasterizer = material.rasterizer,
            // always present, even when the pass overrides it: without it
            // Metal loses the depth format and D3D12 gets DSVFormat UNKNOWN
            .depthStencil =
                RHIDepthStencilState{
                    .format = pass.depthFormat,
                    .depthWriteEnable = state.depthWrite,
                    .depthFunc = state.depthFunc
                },
            .renderTargetCount = pass.renderTargetFormats.size(),
            .profile = material.profile
        };
        if(state.depthBias) {
            desc.rasterizer.depthBias = state.depthBias->depthBias;
            desc.rasterizer.depthBiasClamp = state.depthBias->depthBiasClamp;
            desc.rasterizer.slopeScaledDepthBias =
                state.depthBias->slopeScaledDepthBias;
        }
        // no fragment stage and no blend, so every material that rasterizes
        // alike shares one depth-only pipeline
        if(depthOnly)
            return desc;

        const auto& debug = pass.debug;
        desc.fragmentShader =
            state.fragmentShader.value_or(material.fragmentShader);
        desc.blend = debug.blend ? debug.blend : material.blend;
        if(debug.fillMode)
            desc.rasterizer.fillMode = *debug.fillMode;
        if(debug.depthFunc)
            desc.depthStencil->depthFunc = *debug.depthFunc;
        if(debug.depthWrite)
            desc.depthStencil->depthWriteEnable = *debug.depthWrite;
        std::ranges::copy(
            pass.renderTargetFormats,
            desc.renderTargetFormats.begin()
        );
        // after the depth-only return: a program with no fragment stage calls
        // no model, so every model shares its pipeline
        if(state.linksShading && !material.shadingModule.empty())
            desc.linkedModules.push_back(material.shadingModule);

        return desc;
    }

    PipelineCache::~PipelineCache() = default;

    RHIGraphicsPipelineState& PipelineCache::Resolve(
        const MaterialPipelineDesc& material,
        const PassPipelineDesc& pass
    ) {
        return Resolve(Compose(material, pass));
    }

    RHIGraphicsPipelineState& PipelineCache::Resolve(
        const RHIGraphicsPipelineStateDesc& desc
    ) {
        const auto found = states.find(desc);
        if(found != states.end())
            return *found->second;

        auto state = device.CreatePipelineState(desc);
        const auto [inserted, _] = states.emplace(desc, std::move(state));

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
