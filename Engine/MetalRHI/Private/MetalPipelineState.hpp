#pragma once

#include <Metal/MTLRenderCommandEncoder.hpp>
#include <Metal/MTLComputeCommandEncoder.hpp>
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class MetalGraphicsPipelineState final: public RHIGraphicsPipelineState{
    private:
        NS::SharedPtr<MTL::RenderPipelineState> pipeline;
        RHIRasterizerState rasterizerState{};
        NS::SharedPtr<MTL::DepthStencilState> depthStencilState;

        MTL::PrimitiveType topology = MTL::PrimitiveType::PrimitiveTypeTriangleStrip;

    #if defined(_DEBUG) || !defined(NDEBUG)
        const Str debugName;
    #endif

    public:
        MetalGraphicsPipelineState(
            MTL::Device& device,
            const RHIGraphicsPipelineStateDesc& desc,
            StrView name = {}
        );

        ~MetalGraphicsPipelineState();

        void Bind(MTL::RenderCommandEncoder&);

        MTL::PrimitiveType GetTopology() const noexcept{
            return topology;
        }

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

    #if defined(_DEBUG) || !defined(NDEBUG)
        const Str debugName;
    #endif

    public:
        MetalComputePipelineState(
            MTL::Device& device,
            const RHIComputePipelineStateDesc& desc,
            StrView name = {}
        );

        ~MetalComputePipelineState();

        void Bind(MTL::ComputeCommandEncoder&);

        MTL::Size GetThreadsPerThreadgroup() const{
            return threadsPerThreadgroup;
        }
    };
}
