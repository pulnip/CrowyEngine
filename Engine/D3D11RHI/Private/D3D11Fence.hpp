#pragma once

#include <d3d11.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIFence.hpp"
#endif

namespace Crowy
{
    class D3D11Fence
#ifndef USE_STATIC_RHI
        : public RHIFence
#endif
    {
    public:
        D3D11Fence(
            ID3D11Device& device,
            uint64_t initialValue
        ){}

        void waitCPU(uint64_t waitValue, uint64_t timeoutMs) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
        }

        uint64_t getValue() noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            return 0;
        }

        bool isComplete(uint64_t value) noexcept RHI_OVERRIDE{
            // NOTE. No-Op for D3D11
            return true;
        }
    };
}