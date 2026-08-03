#pragma once

#include <unordered_map>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHIBuffer.hpp"
#include "DX12Definitions.hpp"

namespace Crowy
{
    class DX12Buffer: public RHIBuffer{
    private:
        struct FrameResource{
            BufferRAII buffer = nullptr;
            void* mapped = nullptr;
            // descriptor heap index
            std::unordered_map<RHIBufferViewDesc, UINT> cbvs;
            std::unordered_map<RHIBufferViewDesc, UINT> srvs;
            std::unordered_map<RHIBufferViewDesc, UINT> uavs;
        #if defined(_DEBUG) || !defined(NDEBUG)
            bool slotWritten = false;
        #endif
        };
        // 1 for default heap, RHI_FRAMES_IN_FLIGHT for others.
        std::vector<FrameResource> resources;
        const u64& frameIndex;
        // CBV, SRV, UAV
        DescriptorHeapAllocator& heap;

    #if defined(_DEBUG) || !defined(NDEBUG)
        bool tracksSlotWrites = false;
    #endif

    public:
        DX12Buffer(
            Device&,
            const RHIBufferCreateDesc&,
            const DX12Capabilities&,
            const u64& frameIndex,
            DescriptorHeapAllocator&,
            StrView name
        );

        ~DX12Buffer();

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

        u32 GetSize() const noexcept RHI_OVERRIDE;

        Buffer* Get() noexcept{ return resources[currentIndex()].buffer.Get(); }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress();

        u64 GetReadableID(const RHIBufferViewDesc&) RHI_OVERRIDE;
        u64 GetWritableID(const RHIBufferViewDesc&) RHI_OVERRIDE;

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
