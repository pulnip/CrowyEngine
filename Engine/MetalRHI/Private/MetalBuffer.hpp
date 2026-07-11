#pragma once

#include <Foundation/NSTypes.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLBuffer.hpp>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHIBuffer.hpp"

namespace Crowy
{
    class MetalBuffer final: public RHIBuffer{
    private:
        MTL::Buffer* buffer;

        bool isManaged = false;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        MetalBuffer(
            MTL::Device& device,
            const RHIBufferCreateDesc& desc,
            StrView name = {}
        );
        ~MetalBuffer();

        void Upload(
            const void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE;

        void Download(
            void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE;

        u32 GetSize() const noexcept RHI_OVERRIDE{
            return buffer->length();
        }

        RHIResourceState GetState() const RHI_OVERRIDE{
            return currentState;
        }

        void SetState(RHIResourceState state) RHI_OVERRIDE{
            currentState = state;
        }

        MTL::Buffer* Get() noexcept{ return buffer; }
        const MTL::Buffer* Get() const noexcept{ return buffer; }
    };
}
