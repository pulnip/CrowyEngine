#pragma once

#include <cstddef>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "assert.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBuffer.hpp"
#endif

namespace Crowy
{
    class MetalBuffer
#ifndef USE_STATIC_RHI
        : public RHIBuffer
#endif
    {
    private:
        MTL::Buffer* buffer;
        size_t size = 0;
        RHIBufferUsage usage = RHIBufferUsage::None;
        bool isCPUAccessible = false;
        bool isManaged = false;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        MetalBuffer(
            MTL::Device* device,
            const RHIBufferCreateDesc& desc
        ) noexcept
            :usage(desc.usage),size(desc.size)
        {
            auto hasVertexUsage = hasFlag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = hasFlag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer);
            auto hasCPUWrite = hasFlag(desc.usage, RHIBufferUsage::CPUWrite);

            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage ||
                              hasCPUWrite || desc.initialData != nullptr;

            if(desc.initialData){
            #if TARGET_OS_OSX
                isManaged = true;
                buffer = device->newBuffer(
                    desc.initialData, desc.size,
                    MTL::StorageModeManaged
                );
            #else
                buffer = device->newBuffer(
                    desc.initialData, desc.size,
                    MTL::StorageModeShared
                );
            #endif
            }
            else{
                buffer = device->newBuffer(
                    desc.size,
                    isCPUAccessible ? MTL::StorageModeShared :
                                      MTL::StorageModePrivate
                );
            }

        #if defined(_DEBUG) || !defined(NDEBUG)
            if(!desc.debugName.empty()){
                buffer->setLabel(
                    NS::String::string(desc.debugName.c_str(), NS::UTF8StringEncoding)
                );
            }
        #endif
        }

        ~MetalBuffer(){
            buffer->release();
        }

        void upload(
            const void* data, size_t size,
            size_t offset = 0
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isCPUAccessible);

            void* ptr = buffer->contents();
            memcpy(static_cast<uint8_t*>(ptr) + offset, data, size);

            if(isManaged){
                buffer->didModifyRange(NS::Range::Make(offset, size));
            }
        }

        MTL::Buffer* get() const{ return buffer; }

        RHIResourceState getState() const noexcept RHI_OVERRIDE{
            return currentState;
        }

        void setState(RHIResourceState state) noexcept RHI_OVERRIDE{
            currentState = state;
        }
    };
}
