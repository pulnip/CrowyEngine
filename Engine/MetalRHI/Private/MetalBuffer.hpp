#pragma once

#include <cstddef>
#include <cstring>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "assert.hpp"
#include "ptr_util.hpp"
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
        bool isManaged = false;
        RHIResourceState currentState = RHIResourceState::Common;

    public:
        MetalBuffer(
            MTL::Device& device,
            const RHIBufferCreateDesc& desc,
            const std::string& name = ""
        ) noexcept
            :usage(desc.usage),size(desc.size)
        {
            auto hasVertexUsage = has_flag(desc.usage, RHIBufferUsage::VertexBuffer);
            auto hasIndexUsage = has_flag(desc.usage, RHIBufferUsage::IndexBuffer);
            auto hasConstantUsage = has_flag(desc.usage, RHIBufferUsage::ConstantBuffer);
            auto needCPUAccess = hasVertexUsage || hasIndexUsage || hasConstantUsage || desc.initialData != nullptr;

            auto isGPUOnly = has_flag(desc.access, RHIMemoryAccess::GPUOnly);
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
                    NS::String::string(name.c_str(), NS::UTF8StringEncoding)
                );
            }
        #endif
        }

        ~MetalBuffer(){
            if(buffer != nullptr){
                buffer->release();
                buffer = nullptr;
            }
        }

        inline void upload(
            const void* data, uint32_t size,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(size <= bufSize - offset);
            void* mapped = buffer->contents();

            std::memcpy(
                ptr_add(mapped, offset),
                data,
                size
            );

            if(isManaged){
                buffer->didModifyRange(NS::Range::Make(offset, size));
            }
        }

        inline void download(
            void* data, uint32_t size,
            uint32_t offset = 0
        ) noexcept RHI_OVERRIDE{
            const auto bufSize = this->size;
            CROWY_ASSERT(size <= bufSize - offset);
            void* mapped = buffer->contents();

            std::memcpy(
                data,
                ptr_add(mapped, offset),
                size
            );
        }

        uint32_t getSize() const noexcept RHI_OVERRIDE{
            return size;
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
