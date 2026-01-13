#pragma once

#include <cstddef>
#include <memory>
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
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
            
            if(desc.initialData){
                buffer = device->newBuffer(
                    desc.initialData, desc.size,
                #if TARGET_OS_OSX
                    MTL::StorageModeManaged  // macOS: 성능상 이점 가능
                #else
                    MTL::StorageModeShared   // iOS/iPadOS: 이것만 가능
                #endif
                );
            }
            else{
                buffer = device->newBuffer(
                    desc.size,
                    isCPUAccessible ? MTL::StorageModeShared :
                                      MTL::StorageModePrivate
                );
            }
        }

        ~MetalBuffer(){
            buffer->release();
        }

        void update(
            const void* data, size_t size,
            size_t offset
        ) noexcept RHI_OVERRIDE{
            CROWY_ASSERT(isCPUAccessible);

            void* ptr = buffer->contents();
            memcpy(static_cast<uint8_t*>(ptr) + offset, data, size);

        #if TARGET_OS_OSX
            buffer->didModifyRange(NS::Range::Make(offset, size));
        #endif
        }

        MTL::Buffer* get() const{ return buffer; }
    };
}
