#pragma once

#include <cstddef>
#include <memory>
#include <Metal/Metal.hpp>
#include "RHIAPI.h"
#include "RHIDefinitions.h"
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
    public:
        MetalBuffer(
            MTL::Device* device,
            const RHIBufferCreateDesc& desc
        )
            : usage(desc.usage)
            , size(desc.size)
        {
            auto hasVertexUsage = (desc.usage & BUF_VertexBuffer) != 0;
            auto hasIndexUsage = (desc.usage & BUF_IndexBuffer) != 0;
            auto hasConstantUsage = (desc.usage & BUF_ConstantBuffer) != 0;
            isCPUAccessible = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;
            
            if(desc.initialData){
                buffer = device->newBuffer(
                    desc.initialData, desc.size,
                    MTL::ResourceStorageModeShared
                );
            }
            else{
                buffer = device->newBuffer(
                    desc.initialData, desc.size,
                    isCPUAccessible ? MTL::StorageModeShared :
                                      MTL::StorageModePrivate
                );
            }
        }
        ~MetalBuffer(){
            buffer->release();
        }

    private:
        MTL::Buffer* buffer;
        size_t size = 0;
        RHIBufferUsageFlags usage = RHIBufferUsageFlags::BUF_None;
        bool isCPUAccessible = false;
    };
}
