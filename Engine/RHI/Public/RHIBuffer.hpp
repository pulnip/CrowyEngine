#pragma once

#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #if defined(USE_METAL_BACKEND)
        #include "MetalBuffer.hpp"
    #elif defined(USE_D3D11_BACKEND)
        #include "D3D11Buffer.hpp"
    #else
        #include "NullBuffer.hpp"
    #endif
#endif

namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIBufferType = requires(T buffer){
    };
    static_assert(RHIBufferType<RHIBuffer>);
#else
    class RHIBuffer{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIBuffer)

        virtual void upload(
            const void* data, uint32_t size,
            uint32_t offset = 0
        ) noexcept = 0;

        // Notice! only valid for Metal and D3D11
        virtual void download(
            void* data, uint32_t size,
            uint32_t offset = 0
        ) noexcept = 0;

        virtual uint32_t getSize() const noexcept = 0;

        virtual RHIResourceState getState() const noexcept = 0;
        virtual void setState(RHIResourceState state) noexcept = 0;
    };
#endif
}
