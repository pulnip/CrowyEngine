#pragma once

#include <Metal/MTLTypes.hpp>
#include <Metal/MTLArgument.hpp>
#include <Metal/MTLBuffer.hpp>
#include <Metal/MTLComputePipeline.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/Metal.hpp>
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class MetalGraphicsPipelineState final: public RHIGraphicsPipelineState{
    private:
        MTL::RenderPipelineState* pipeline = nullptr;
        RHIRasterizerState rasterizerState{};
        MTL::DepthStencilState* depthStencilState = nullptr;

        MTL::PrimitiveType topology = MTL::PrimitiveType::PrimitiveTypeTriangleStrip;

        const Str debugName;

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
        MTL::Function* cs = nullptr;
        MTL::ComputePipelineState* pipeline = nullptr;
        MTL::Size threadsPerThreadgroup = {0, 0, 0};

        const Str debugName;

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

    private:
        static MTL::Size DefaultGroupSize(
            u32 numThreads,
            const Size3D& gridSize
        ) noexcept;
    };
}
