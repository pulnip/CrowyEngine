#pragma once

#include <map>
#include "DX12Definitions.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct RootSignatureLayout{
        RootSignatureRAII rootSignature = nullptr;
        std::map<std::pair<UINT, UINT>, UINT> cbvRootIndex;
    };

    class DX12GraphicsPipelineState{
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
        // UINT GetCBVRootIndex(UINT reg, UINT space);
    };

    class DX12ComputePipelineState{
    private:
        PipelineStateRAII pipeline = nullptr;

        RHIComputeBindingInfo bindingInfo;
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
        // UINT GetCBVRootIndex(UINT reg, UINT space);

        Size3D getThreadGroupSize() const{
            return threadGroupSize;
        }
    };
}
