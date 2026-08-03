#pragma once

#include <vector>
#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLComputeCommandEncoder.hpp>
#include <Metal/MTLSampler.hpp>
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class MetalReservedSamplers;

    // a sampler resolved to its encoder slot
    struct MetalSamplerBinding{
        NS::UInteger slot;
        // owned by MetalDevice's sampler table,
        // which outlives every pipeline state
        MTL::SamplerState* sampler;
    };

    class MetalGraphicsPipelineState final: public RHIGraphicsPipelineState{
    private:
        NS::SharedPtr<MTL::RenderPipelineState> pipeline;
        RHIRasterizerState rasterizerState{};
        NS::SharedPtr<MTL::DepthStencilState> depthStencilState;

        MTL::PrimitiveType topology = MTL::PrimitiveType::PrimitiveTypeTriangleStrip;

        std::vector<MetalSamplerBinding> vsSamplers;
        std::vector<MetalSamplerBinding> fsSamplers;

        // buffer-argument-table usage per stage, bit i = buffer index i;
        // the table has 31 entries, so u32 covers it
        u32 vsUsedBuffers = 0;
        u32 fsUsedBuffers = 0;

    #if defined(_DEBUG) || !defined(NDEBUG)
        const Str debugName;
    #endif

    public:
        MetalGraphicsPipelineState(
            MTL::Device&,
            MetalReservedSamplers&,
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        );

        ~MetalGraphicsPipelineState();

        void Bind(MTL::RenderCommandEncoder&);

        MTL::PrimitiveType GetTopology() const noexcept{
            return topology;
        }

        u32 GetVSUsedBufferMask() const noexcept{ return vsUsedBuffers; }
        u32 GetFSUsedBufferMask() const noexcept{ return fsUsedBuffers; }

    private:
        void createDepthStencilState(
            MTL::Device& device,
            const RHIDepthStencilState& desc
        );
    };

    class MetalComputePipelineState final: public RHIComputePipelineState{
    private:
        NS::SharedPtr<MTL::ComputePipelineState> pipeline;
        MTL::Size threadsPerThreadgroup = {0, 0, 0};

        std::vector<MetalSamplerBinding> samplers;

    #if defined(_DEBUG) || !defined(NDEBUG)
        const Str debugName;
    #endif

    public:
        MetalComputePipelineState(
            MTL::Device&,
            MetalReservedSamplers&,
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        );

        ~MetalComputePipelineState();

        void Bind(MTL::ComputeCommandEncoder&);

        MTL::Size GetThreadsPerThreadgroup() const{
            return threadsPerThreadgroup;
        }
    };
}
