#include <cstring>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "MetalBuffer.hpp"
#include "MetalUtil.hpp"
#include "PtrUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    namespace{
        struct BufferPolicy{
            u32 slotCount;
            bool persistentMap;
            RHIBarrierSync sync;
            RHIBarrierAccess access;
        };

        auto resolve(
            RHIBufferUsage usage,
            RHIMemoryAccess access
        ){
            using enum RHIMemoryAccess;
            using enum RHIBufferUsage;

            switch(access){
            case GPUOnly:
                return BufferPolicy{
                    .slotCount = 1,
                    .persistentMap = false,
                    .sync = RHIBarrierSync::None,
                    .access = RHIBarrierAccess::NoAccess
                };
            case CPUWrite:
                return BufferPolicy{
                    .slotCount = usage == CopySrc ?
                        1 :
                        RHI_FRAMES_IN_FLIGHT,
                    .persistentMap = true,
                    .sync = RHIBarrierSync::None,
                    .access = RHIBarrierAccess::ConstantBuffer
                };
            case CPURead:
                return BufferPolicy{
                    .slotCount = RHI_FRAMES_IN_FLIGHT,
                    .persistentMap = true,
                    .sync = RHIBarrierSync::Copy,
                    .access = RHIBarrierAccess::CopyDst
                };
            case Transient:
                CROWY_ASSERT(false, "Transient is texture-only");
            default:
                std::unreachable();
            }
        }
    }

    MetalBuffer::MetalBuffer(
        MTL::Device& device,
        const RHIBufferCreateDesc& desc,
        const u64& frameIndex,
        StrView name
    )
        : frameIndex(frameIndex)
    {
        using enum RHIBufferUsage;
        using enum RHIMemoryAccess;

        const auto policy = resolve(
            desc.usage,
            desc.access
        );

        const auto hasConstantUsage = hasFlag(desc.usage, ConstantBuffer);
        const auto isUnorderedAccess = hasFlag(desc.usage, UnorderedAccess);
        const auto isCopyDst = hasFlag(desc.usage, CopyDst);

        const auto isCPUWrite = (desc.access == CPUWrite);
        CROWY_ASSERT(!isCPUWrite || (!isUnorderedAccess && !isCopyDst));
        const auto isCPURead  = (desc.access == CPURead);
        CROWY_ASSERT(!isCPURead || (desc.usage == CopyDst));
        const auto isGPUOnly  = (desc.access == GPUOnly);

        resources.reserve(policy.slotCount);
        if(desc.initialData != nullptr){
            for(u32 i=0; i<policy.slotCount; ++i){
                FrameResource resource{
                    .buffer = device.newBuffer(
                        desc.initialData,
                        desc.size,
                        isGPUOnly ?
                            MTL::ResourceStorageModePrivate :
                            MTL::ResourceStorageModeManaged
                    ),
                    .syncState = policy.sync,
                    .accessState = policy.access,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .slotWritten = false
                #endif
                };
                if(policy.persistentMap){
                    CROWY_ASSERT(isCPUWrite || isCPURead);
                    resource.mapped = resource.buffer->contents();
                }

            #if defined(_DEBUG) || !defined(NDEBUG)
                if(!name.empty()){
                    resource.buffer->setLabel(toNSString(name));
                }
            #endif

                resources.emplace_back(std::move(resource));
            }
        }
        else{
            for(u32 i=0; i<policy.slotCount; ++i){
                FrameResource resource{
                    .buffer = device.newBuffer(
                        desc.size,
                        isGPUOnly ?
                            MTL::ResourceStorageModePrivate :
                            MTL::ResourceStorageModeManaged
                    ),
                    .syncState = policy.sync,
                    .accessState = policy.access,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .slotWritten = false
                #endif
                };
                if(policy.persistentMap){
                    CROWY_ASSERT(isCPUWrite || isCPURead);
                    resource.mapped = resource.buffer->contents();
                }

            #if defined(_DEBUG) || !defined(NDEBUG)
                if(!name.empty()){
                    resource.buffer->setLabel(toNSString(name));
                }
            #endif

                resources.emplace_back(std::move(resource));
            }
        }
    #if defined(_DEBUG) || !defined(NDEBUG)
        tracksSlotWrites = isCPUWrite && resources.size() > 1;
    #endif
    }

    MetalBuffer::~MetalBuffer(){
        for(u32 i=0; i<resources.size(); ++i){
            auto& resource = resources[i];

            if(resource.buffer != nullptr){
                resource.buffer->release();
                resource.buffer = nullptr;
            }

        }
    }

    void MetalBuffer::upload(
        u32 index,
        const void* data,
        u32 size,
        u32 offset
    ){
        CROWY_ASSERT(size + offset <= GetSize());
        auto& resource = resources[index];

        std::memcpy(
            ptrAdd(resource.mapped, offset),
            data,
            size
        );

        resource.buffer->didModifyRange(
            NS::Range::Make(
                offset,
                size
            )
        );
    }

    void MetalBuffer::download(
        u32 index,
        void* data,
        u32 size,
        u32 offset
    ){
        CROWY_ASSERT(offset + size <= GetSize());
        auto& resource = resources[index];

        std::memcpy(
            data,
            ptrAdd(resource.mapped, offset),
            size
        );
    }
}
