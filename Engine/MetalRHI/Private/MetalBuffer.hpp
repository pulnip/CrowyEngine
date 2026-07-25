#pragma once

#include <Foundation/NSTypes.hpp>
#include <Metal/MTLDevice.hpp>
#include <Metal/MTLBuffer.hpp>
#include "RHIAPI.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    class MetalBuffer final: public RHIBuffer{
    private:
        struct FrameResource{
            MTL::Buffer* buffer;
            // for Enhanced Resource Barrier
            RHIBarrierSync syncState;
            RHIBarrierAccess accessState;
        #if defined(_DEBUG) || !defined(NDEBUG)
            bool slotWritten = false;
        #endif
        };
        std::vector<FrameResource> resources;
        const u64& frameIndex;

    #if defined(_DEBUG) || !defined(NDEBUG)
        bool tracksSlotWrites = false;
    #endif

    public:
        MetalBuffer(
            MTL::Device& device,
            const RHIBufferCreateDesc& desc,
            StrView name = {}
        );
        ~MetalBuffer();

        void Upload(
            const void* src,
            u32 srcSize,
            u32 offset = 0
        ) RHI_OVERRIDE{
            upload(
                currentIndex(),
                src,
                srcSize,
                offset
            );
        }
        void UploadAll(
            const void* src,
            u32 srcSize,
            u32 offset = 0
        ) RHI_OVERRIDE{
            for(u32 i=0; i<resources.size(); ++i){
                upload(
                    i,
                    src,
                    srcSize,
                    offset
                );
            }
        }

        void Download(
            void* dst,
            u32 dstSize,
            u32 offset = 0
        ) RHI_OVERRIDE{
            download(
                currentIndex(),
                dst,
                dstSize,
                offset
            );
        }

        u32 GetSize() const noexcept RHI_OVERRIDE{
            return resources[currentIndex()].buffer->length();
        }

        RHIBarrierSync GetSyncState() const RHI_OVERRIDE{
            auto& frameResource = resources[currentIndex()];
            return frameResource.syncState;
        }
        RHIBarrierSync TransitionState(RHIBarrierSync newState) RHI_OVERRIDE{
            auto& frameResource = resources[currentIndex()];

            const auto oldState = frameResource.syncState;
            frameResource.syncState = newState;
            return oldState;
        }
        RHIBarrierAccess GetAccessState() const RHI_OVERRIDE{
            auto& frameResource = resources[currentIndex()];
            return frameResource.accessState;
        }
        RHIBarrierAccess TransitionState(RHIBarrierAccess newState) RHI_OVERRIDE{
            auto& frameResource = resources[currentIndex()];

            const auto oldState = frameResource.accessState;
            frameResource.accessState = newState;
            return oldState;
        }

        u64 GetReadableID(const RHIBufferViewDesc&) RHI_OVERRIDE;
        u64 GetWritableID(const RHIBufferViewDesc&) RHI_OVERRIDE;

        MTL::Buffer* Get() noexcept{ return resources[currentIndex()].buffer; }

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
            const void* src,
            u32 srcSize,
            u32 offset = 0
        );
        void download(
            u32 index,
            void* dst,
            u32 dstSize,
            u32 offset = 0
        );
    };
}
