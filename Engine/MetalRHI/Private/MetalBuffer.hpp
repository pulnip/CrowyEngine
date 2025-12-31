#pragma once

#include <cstddef>
#include <memory>
#include <Metal/Metal.hpp>
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
        )
            : usage(desc.usage)
            , size(desc.size)
        {
            auto hasVertexUsage = hasFlag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = hasFlag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = hasFlag(desc.usage, RHIBufferUsage::ConstantBuffer);
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
            
            if(desc.initialData){
                buffer = device->newBuffer(
                    desc.initialData, desc.size,
                    MTL::ResourceStorageModeShared
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

        MTL::Buffer* get() const{ return buffer; }
    };
}
