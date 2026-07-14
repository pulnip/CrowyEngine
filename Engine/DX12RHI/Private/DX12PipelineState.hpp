#pragma once

#include "DX12Definitions.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class DX12GraphicsPipelineState: public RHIGraphicsPipelineState{
    private:
        PipelineStateRAII pipeline = nullptr;

        D3D_PRIMITIVE_TOPOLOGY primitiveTopology = D3D_PRIMITIVE_TOPOLOGY_UNDEFINED;

    #if defined(_DEBUG) || !defined(NDEBUG)
        const Str debugName;
    #endif

    public:
        DX12GraphicsPipelineState(
            Device&,
            const RHIGraphicsPipelineStateDesc&,
            RootSignature&,
            StrView name = {}
        );

        ~DX12GraphicsPipelineState();

        void Bind(CommandList&) const;
    };

    class DX12ComputePipelineState: public RHIComputePipelineState{
    private:
        PipelineStateRAII pipeline = nullptr;

        Size3D threadGroupSize = {256, 1, 1};

    #if defined(_DEBUG) || !defined(NDEBUG)
        const std::string debugName;
    #endif

    public:
        DX12ComputePipelineState(
            Device&,
            const RHIComputePipelineStateDesc&,
            RootSignature&,
            StrView name = {}
        );

        ~DX12ComputePipelineState();

        void Bind(CommandList&) const;

        Size3D getThreadGroupSize() const{
            return threadGroupSize;
        }
    };
}
