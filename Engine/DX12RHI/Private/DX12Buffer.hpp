#pragma once

#include <unordered_map>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#include "RHIBuffer.hpp"
#include "DX12Definitions.hpp"

#include <stdexcept>

namespace Crowy
{
    class DX12Buffer: public RHIBuffer{
    private:
        struct FrameResource{
            BufferRAII buffer = nullptr;
            void* mapped = nullptr;
            // for Enhanced Resource Barrier
            D3D12_BARRIER_SYNC syncState = D3D12_BARRIER_SYNC_NONE;
            D3D12_BARRIER_ACCESS accessState = D3D12_BARRIER_ACCESS_COMMON;
            // No Layout Barrier for Buffer (trivially Row-major Layout)
            // descriptor heap index
            std::unordered_map<RHIBufferViewDesc, UINT> cbvs;
            std::unordered_map<RHIBufferViewDesc, UINT> srvs;
            std::unordered_map<RHIBufferViewDesc, UINT> uavs;
        };
        // 1 for default heap, RHI_FRAMES_IN_FLIGHT for others.
        std::vector<FrameResource> resources;
        const u64& frameIndex;
        // CBV, SRV, UAV
        DescriptorHeapAllocator& heap;

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

        RHIResourceState GetState() const RHI_OVERRIDE{
            throw std::runtime_error("Unimplemented");
        }
        void SetState(RHIResourceState) RHI_OVERRIDE{
            throw std::runtime_error("Unimplemented");
        }

        // Pipeline Stage Scope
        auto TransitionState(D3D12_BARRIER_SYNC newState) noexcept{
            auto& frameResource = resources[currentIndex()];

            const auto oldState = frameResource.syncState;
            frameResource.syncState = newState;
            return oldState;
        }
        // Cache Visibility
        auto TransitionState(D3D12_BARRIER_ACCESS newState) noexcept{
            auto& frameResource = resources[currentIndex()];

            const auto oldState = frameResource.accessState;
            frameResource.accessState = newState;
            return oldState;
        }

        Buffer* Get() noexcept{ return resources[currentIndex()].buffer.Get(); }

        D3D12_GPU_VIRTUAL_ADDRESS GetGPUAddress();

        UINT GetOrCreateCBV(const RHIBufferViewDesc&);
        UINT GetOrCreateSRV(const RHIBufferViewDesc&);
        UINT GetOrCreateUAV(const RHIBufferViewDesc&);

        UINT GetOrCreateCBV(){
            return GetOrCreateCBV(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::RawConfig{}
            });
        }
        UINT GetOrCreateSRV(){
            return GetOrCreateSRV(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::RawConfig{}
            });
        }
        UINT GetOrCreateUAV(){
            return GetOrCreateUAV(RHIBufferViewDesc{
                .offset = 0,
                .size = GetSize(),
                .config = RHIBufferViewDesc::RawConfig{}
            });
        }

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
