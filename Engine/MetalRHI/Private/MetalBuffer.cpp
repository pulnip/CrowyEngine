#include <cstring>
#include <Metal/Metal.hpp>
#include <TargetConditionals.h>
#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "IntMath.hpp"
#include "MetalBuffer.hpp"
#include "MetalUtil.hpp"
#include "PtrUtil.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    namespace{
        struct BufferPolicy{
            RHIMemoryClass memoryClass;
            u32 slotCount;
            bool persistentMap;
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
                    .memoryClass = RHIMemoryClass::Device,
                    .slotCount = 1,
                    .persistentMap = false
                };
            case CPUWrite:
                return BufferPolicy{
                    .memoryClass = RHIMemoryClass::Upload,
                    .slotCount = usage == CopySrc ?
                        1 :
                        RHI_FRAMES_IN_FLIGHT,
                    .persistentMap = true
                };
            case CPURead:
                return BufferPolicy{
                    .memoryClass = RHIMemoryClass::Readback,
                    .slotCount = RHI_FRAMES_IN_FLIGHT,
                    .persistentMap = true
                };
            case Transient:
                CROWY_ASSERT(false, "Transient is texture-only");
            default:
                std::unreachable();
            }
        }
    }

    MetalBuffer::MetalBuffer(
        MetalAllocator& allocator,
        const RHIBufferCreateDesc& desc,
        const u64& frameIndex,
        StrView name
    )
        : frameIndex(frameIndex)
        , allocator(allocator)
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
        for(u32 i=0; i<policy.slotCount; ++i){
            FrameResource resource{
                .allocation = allocator.AllocateBuffer(
                    nextMul(desc.size, 16u),
                    policy.memoryClass,
                    name
                )
            #if defined(_DEBUG) || !defined(NDEBUG)
                , .slotWritten = false
            #endif
            };
            resource.buffer = static_cast<MTL::Buffer*>(resource.allocation.resource);

            if(policy.persistentMap){
                CROWY_ASSERT(isCPUWrite || isCPURead);
                resource.mapped = resource.buffer->contents();
            }

            resources.emplace_back(std::move(resource));
        }

        if(desc.initialData != nullptr && isCPUWrite){
            UploadAll(desc.initialData, desc.size);
        }

    #if defined(_DEBUG) || !defined(NDEBUG)
        tracksSlotWrites = isCPUWrite && resources.size() > 1;
    #endif
    }

    MetalBuffer::~MetalBuffer(){
        for(auto& resource: resources){
            resource.buffer = nullptr;
            allocator.Free(resource.allocation);
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

    u64 MetalBuffer::getResourceID(const RHIBufferViewDesc&){
        return resources[currentIndex()].buffer->gpuAddress();
    }
}
