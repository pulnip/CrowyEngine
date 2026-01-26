#pragma once

#include <utility>
#include <d3d11.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISampler.hpp"
#endif

namespace Crowy
{
    static D3D11_TEXTURE_ADDRESS_MODE convertAddressMode(RHIAddressMode mode){
        switch(mode){
        case RHIAddressMode::Wrap  : return D3D11_TEXTURE_ADDRESS_WRAP;
        case RHIAddressMode::Clamp : return D3D11_TEXTURE_ADDRESS_CLAMP;
        case RHIAddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
        case RHIAddressMode::Border: return D3D11_TEXTURE_ADDRESS_BORDER;
        default:
            std::unreachable();
        }
    }

    static D3D11_FILTER convertFilter(
        RHIFilter min, RHIFilter mag, RHIFilter mip,
        bool anisotropy, bool comparison
    ){
        if(anisotropy)
            return comparison ?
                D3D11_FILTER_COMPARISON_ANISOTROPIC :
                D3D11_FILTER_ANISOTROPIC;

        UINT flags = 0;

        if(mip == RHIFilter::Linear) flags |= 0x1;
        if(mag == RHIFilter::Linear) flags |= 0x4;
        if(min == RHIFilter::Linear) flags |= 0x10;
        if(comparison)               flags |= 0x80;

        return static_cast<D3D11_FILTER>(flags);
    }

    class D3D11Sampler
#ifndef USE_STATIC_RHI
        : public RHISampler
#endif
    {
    private:
        ID3D11SamplerState* sampler = nullptr;

    public:
        D3D11Sampler(
            ID3D11Device* device,
            const RHISamplerState& desc
        ){
            D3D11_SAMPLER_DESC samplerDesc{
                .Filter = convertFilter(
                    desc.minFilter, desc.magFilter, desc.mipFilter,
                    desc.maxAnisotropy > 1,
                    desc.compareFunc != RHIComparisonFunc::Never
                ),
                .AddressU = convertAddressMode(desc.addressU),
                .AddressV = convertAddressMode(desc.addressV),
                .AddressW = convertAddressMode(desc.addressW),
                .MipLODBias = desc.mipLODBias,
                .MaxAnisotropy = desc.maxAnisotropy,
                .ComparisonFunc = convertCompareFunc(desc.compareFunc),
                .MinLOD = desc.minLOD,
                .MaxLOD = desc.maxLOD
            };
            if(FAILED(device->CreateSamplerState(&samplerDesc, &sampler))){
                throw std::runtime_error("Failed to create D3D11 Sampler");
            }
        }

        ~D3D11Sampler(){
            if(sampler != nullptr){
                sampler->Release();
                sampler = nullptr;
            }
        }

        ID3D11SamplerState* get() const{ return sampler; }
    };
}