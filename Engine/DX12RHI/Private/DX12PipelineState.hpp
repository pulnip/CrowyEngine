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

        Size3D threadGroupSize = {0, 0, 0};

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
