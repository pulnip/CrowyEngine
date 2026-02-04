#pragma once

#include <utility>
#include <d3d12.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISampler.hpp"
#endif
#include "D3D12Util.hpp"
#include "DescriptorHeapAllocator.hpp"

namespace Crowy
{
    static D3D12_TEXTURE_ADDRESS_MODE convert(RHIAddressMode mode){
        switch(mode){
        case RHIAddressMode::Wrap  : return D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        case RHIAddressMode::Clamp : return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        case RHIAddressMode::Mirror: return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;
        case RHIAddressMode::Border: return D3D12_TEXTURE_ADDRESS_MODE_BORDER;
        default:
            std::unreachable();
        }
    }

    static D3D12_FILTER convert(
        RHIFilter min, RHIFilter mag, RHIFilter mip,
        bool anisotropy, bool comparison,
        D3D12_FILTER_REDUCTION_TYPE reduction = D3D12_FILTER_REDUCTION_TYPE_STANDARD
    ){
        if(anisotropy)
            return comparison ?
                D3D12_FILTER_COMPARISON_ANISOTROPIC :
                D3D12_FILTER_ANISOTROPIC;

        UINT flags = 0;

        if(mip == RHIFilter::Linear) flags |= 0x1;
        if(mag == RHIFilter::Linear) flags |= 0x4;
        if(min == RHIFilter::Linear) flags |= 0x10;
        if(comparison)               flags |= 0x80;
        else flags |= (reduction << 8);  // MINIMUM=0x100, MAXIMUM=0x180

        return static_cast<D3D12_FILTER>(flags);
    }

    class D3D12Sampler
#ifndef USE_STATIC_RHI
        : public RHISampler
#endif
    {
    private:
        DescriptorHeapAllocator* allocator = nullptr;
        UINT index;

    public:
        D3D12Sampler(
            DescriptorHeapAllocator* allocator,
            const RHISamplerState& desc
        )
            : allocator(allocator)
        {
            D3D12_SAMPLER_DESC samplerDesc{
                .Filter = convert(
                    desc.minFilter, desc.magFilter, desc.mipFilter,
                    desc.maxAnisotropy > 1,
                    desc.compareFunc != RHIComparisonFunc::Never
                ),
                .AddressU = convert(desc.addressU),
                .AddressV = convert(desc.addressV),
                .AddressW = convert(desc.addressW),
                .MipLODBias = desc.mipLODBias,
                .MaxAnisotropy = desc.maxAnisotropy,
                .ComparisonFunc = convert(desc.compareFunc),
                .BorderColor = {
                    desc.borderColor[0], desc.borderColor[1],
                    desc.borderColor[2], desc.borderColor[3]
                },
                .MinLOD = desc.minLOD,
                .MaxLOD = desc.maxLOD
            };

            index = allocator->allocate(samplerDesc);
        }

        ~D3D12Sampler(){
            if(allocator != nullptr){
                allocator->free(index);
                allocator = nullptr;
            }
        }

        UINT getHeapIndex() const{ return index; }
    };
}