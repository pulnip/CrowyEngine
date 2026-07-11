#include <utility>
#include "DescriptorHeapAllocator.hpp"
#include "DX12Sampler.hpp"
#include "DX12Util.hpp"
#include "RHIDefinitions.hpp"

namespace{
    auto convert(Crowy::RHIAddressMode mode){
        using enum Crowy::RHIAddressMode;

        switch(mode){
        case Wrap  : return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case Clamp : return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:
            std::unreachable();
        }
    }

    auto convert(
        Crowy::RHIFilter min, Crowy::RHIFilter mag, Crowy::RHIFilter mip,
        bool anisotropy, bool comparison
    ){
        using enum Crowy::RHIFilter;

        if(anisotropy)
            return comparison ?
                D3D12_FILTER_COMPARISON_ANISOTROPIC :
                D3D12_FILTER_ANISOTROPIC;

        UINT flags = 0;

        if(mip == Linear) flags |= 0x1;
        if(mag == Linear) flags |= 0x4;
        if(min == Linear) flags |= 0x10;
        if(comparison)               flags |= 0x80;

        return static_cast<D3D12_FILTER>(flags);
    }
}

namespace Crowy
{
    DX12Sampler::DX12Sampler(
        const RHISamplerState& desc,
        DescriptorHeapAllocator& samplerHeap
    )
        : samplerHeap(samplerHeap)
    {
        D3D12_SAMPLER_DESC samplerDesc{
            .Filter = ::convert(
                desc.minFilter, desc.magFilter, desc.mipFilter,
                desc.maxAnisotropy > 1,
                desc.compareFunc != RHIComparisonFunc::Never
            ),
            .AddressU = ::convert(desc.addressU),
            .AddressV = ::convert(desc.addressV),
            .AddressW = ::convert(desc.addressW),
            .MipLODBias = desc.mipLODBias,
            .MaxAnisotropy = desc.maxAnisotropy,
            .ComparisonFunc = convert(desc.compareFunc),
            .BorderColor = {
                desc.borderColor[0],
                desc.borderColor[1],
                desc.borderColor[2],
                desc.borderColor[3]
            },
            .MinLOD = desc.minLOD,
            .MaxLOD = desc.maxLOD
        };

        heapIndex = samplerHeap.Allocate(samplerDesc);
    }

    DX12Sampler::~DX12Sampler(){
        if(heapIndex != UINT_MAX){
            samplerHeap.Free(heapIndex);
            heapIndex = UINT_MAX;
        }
    }
}
