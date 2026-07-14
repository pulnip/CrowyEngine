#include <cstring>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "MetalBuffer.hpp"
#include "PtrUtil.hpp"

namespace Crowy
{
    MetalBuffer::MetalBuffer(
        MTL::Device& device,
        const RHIBufferCreateDesc& desc,
        StrView name
    ){
        auto hasVertexUsage = hasFlag(desc.usage, RHIBufferUsage::VertexBuffer);
        auto hasIndexUsage = hasFlag(desc.usage, RHIBufferUsage::IndexBuffer);
        auto hasConstantUsage = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer);
        auto needCPUAccess = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;

        auto isGPUOnly = hasFlag(desc.access, RHIMemoryAccess::GPUOnly);
        CROWY_ASSERT(!needCPUAccess || !isGPUOnly);

        if(desc.initialData != nullptr){
        #if TARGET_OS_OSX
            isManaged = true;
            buffer = device.newBuffer(
                desc.initialData, desc.size,
                isGPUOnly ?
                    MTL::ResourceStorageModePrivate :
                    MTL::ResourceStorageModeManaged
            );
        #else
            buffer = device.newBuffer(
                desc.initialData, desc.size,
                isGPUOnly ?
                    MTL::ResourceStorageModePrivate :
                    MTL::ResourceStorageModeShared
            );
        #endif
        }
        else{
            buffer = device.newBuffer(
                desc.size,
                isGPUOnly ?
                    MTL::ResourceStorageModePrivate :
                    MTL::ResourceStorageModeShared
            );
        }

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            buffer->setLabel(
                NS::String::string(name.data(), NS::UTF8StringEncoding)
            );
        }
    #endif
    }

    MetalBuffer::~MetalBuffer(){
        if(buffer != nullptr){
            buffer->release();
            buffer = nullptr;
        }
    }

    void MetalBuffer::Upload(
        const void* data,
        u32 size,
        u32 offset
    ){
        CROWY_ASSERT(offset + size <= GetSize());
        void* mapped = buffer->contents();

        std::memcpy(
            ptrAdd(mapped, offset),
            data,
            size
        );

        if(isManaged){
            buffer->didModifyRange(NS::Range::Make(offset, size));
        }
    }

    void MetalBuffer::Download(
        void* data,
        u32 size,
        u32 offset
    ){
        CROWY_ASSERT(offset + size <= GetSize());
        void* mapped = buffer->contents();

        std::memcpy(
            data,
            ptrAdd(mapped, offset),
            size
        );
    }
}
