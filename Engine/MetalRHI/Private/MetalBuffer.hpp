#pragma once

#include <Foundation/NSTypes.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLBuffer.hpp>
#include "MetalAllocator.hpp"
#include "RHIAPI.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class MetalBuffer final: public RHIBuffer{
    private:
        struct FrameResource{
            RHIAllocation allocation{};
            NS::SharedPtr<MTL::Buffer> buffer;
            void* mapped = nullptr;
        #if defined(_DEBUG) || !defined(NDEBUG)
            bool slotWritten = false;
        #endif
        };
        std::vector<FrameResource> resources;
        const u64& frameIndex;
        MetalAllocator& allocator;

    #if defined(_DEBUG) || !defined(NDEBUG)
        bool tracksSlotWrites = false;
    #endif

    public:
        MetalBuffer(
            MetalAllocator&,
            const RHIBufferCreateDesc&,
            const u64& frameIndex,
            StrView name = {}
        );
        ~MetalBuffer();

        void Upload(
            const void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE{
            upload(
                currentIndex(),
                data,
                size,
                offset
            );
        }
        void UploadAll(
            const void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE{
            for(u32 i=0; i<resources.size(); ++i){
                upload(
                    i,
                    data,
                    size,
                    offset
                );
            }
        }

        void Download(
            void* data,
            u32 size,
            u32 offset = 0
        ) RHI_OVERRIDE{
            download(
                currentIndex(),
                data,
                size,
                offset
            );
        }

        u32 GetSize() const noexcept RHI_OVERRIDE{
            return resources[currentIndex()].buffer->length();
        }

        u64 GetReadableID(const RHIBufferViewDesc& view) RHI_OVERRIDE{
            return getResourceID(view);
        }
        u64 GetWritableID(const RHIBufferViewDesc& view) RHI_OVERRIDE{
            return getResourceID(view);
        }

        MTL::Buffer* Get() noexcept{ return resources[currentIndex()].buffer.get(); }

    #if defined(_DEBUG) || !defined(NDEBUG)
        // call wherever the GPU is about to be pointed at the current slot
        void AssertSlotWritten() const noexcept{
            if(!tracksSlotWrites)
                return;

            auto index = currentIndex();
            CROWY_ASSERT(resources[index].slotWritten,
                "slot {} was never written.",
                index
            );
        }
    #endif

    private:
        u32 currentIndex() const noexcept{
            return static_cast<u32>(
                frameIndex % resources.size()
            );
        }

        void upload(
            u32 index,
            const void* data,
            u32 size,
            u32 offset = 0
        );
        void download(
            u32 index,
            void* data,
            u32 size,
            u32 offset = 0
        );

        u64 getResourceID(const RHIBufferViewDesc&);
    };
}
