#include <d3dx12/d3dx12_core.h>
#include "Assert.hpp"
#include "DescriptorHeapAllocator.hpp"
#include "DX12Buffer.hpp"
#include "DX12Definitions.hpp"
#include "DX12Util.hpp"
#include "EnumUtil.hpp"
#include "IntMath.hpp"
#include "PtrUtil.hpp"
#include "VariantUtil.hpp"

namespace{
    struct BufferPolicy{
        Crowy::RHIMemoryClass memoryClass;
        Crowy::u32 slotCount;
        bool persistentMap;
    };

    auto Resolve(
        Crowy::RHIBufferUsage usage,
        Crowy::RHIMemoryAccess access
    ){
        using namespace Crowy;
        using enum RHIMemoryAccess;

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
                .slotCount = usage == RHIBufferUsage::CopySrc ?
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

namespace Crowy
{
    DX12Buffer::DX12Buffer(
        DX12Allocator& allocator,
        const RHIBufferCreateDesc& desc,
        const u64& frameIndex,
        DescriptorHeapAllocator& heap,
        StrView name
    )
        : frameIndex(frameIndex)
        , allocator(allocator)
        , heap(heap)
    {
        using enum RHIBufferUsage;
        using enum RHIMemoryAccess;

        const auto policy = Resolve(desc.usage, desc.access);

        const auto hasConstantUsage = hasFlag(desc.usage, ConstantBuffer);
        const auto isUnorderedAccess = hasFlag(desc.usage, UnorderedAccess);
        const auto isCopyDst = hasFlag(desc.usage, CopyDst);

        const auto isCPUWrite = (desc.access == CPUWrite);
        CROWY_ASSERT(!isCPUWrite || (!isUnorderedAccess && !isCopyDst));
        const auto isCPURead  = (desc.access == CPURead);
        CROWY_ASSERT(!isCPURead || (desc.usage == CopyDst));
        const auto isGPUOnly  = (desc.access == GPUOnly);

        const auto bufDesc = CD3DX12_RESOURCE_DESC1::Buffer(
            D3D12_RESOURCE_ALLOCATION_INFO{
                .SizeInBytes = hasConstantUsage ?
                    nextMul<u32>(desc.size, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT) :
                    desc.size,
                .Alignment = 0
            },
            isUnorderedAccess ?
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS :
                D3D12_RESOURCE_FLAG_NONE
        );

        resources.reserve(policy.slotCount);
        for(u32 i=0; i<policy.slotCount; ++i){
            FrameResource frameResource{
                .allocation = allocator.Allocate(
                    bufDesc,
                    policy.memoryClass,
                    nullptr,
                    name
                )
            #if defined(_DEBUG) || !defined(NDEBUG)
                , .slotWritten = false
            #endif
            };

            if(policy.persistentMap){
                CROWY_ASSERT(isCPUWrite || isCPURead);
                const CD3DX12_RANGE noRead(0, 0);
                CHECK_HRESULT(frameResource.allocation.resource->Map(
                    0,
                    &noRead,
                    &frameResource.mapped
                ), "Failed to Map DX12 Buffer");
            }

            resources.emplace_back(std::move(frameResource));
        }

    #if defined(_DEBUG) || !defined(NDEBUG)
        tracksSlotWrites = isCPUWrite && resources.size() > 1;
    #endif

        if(desc.initialData != nullptr){
            CROWY_ASSERT(!isCPURead);

            if(isCPUWrite){
                UploadAll(
                    desc.initialData,
                    desc.size
                );
            }
        }
    }

    DX12Buffer::~DX12Buffer(){
        for(u32 i=0; i<resources.size(); ++i){
            auto& frameResource = resources[i];

            if(frameResource.mapped != nullptr){
                frameResource.allocation.resource->Unmap(
                    0,
                    nullptr
                );
                frameResource.mapped = nullptr;
            }

            for(const auto& [_, idx]: frameResource.cbvs){
                heap.Free(idx);
            }
            for(const auto& [_, idx]: frameResource.srvs){
                heap.Free(idx);
            }
            for(const auto& [_, idx]: frameResource.uavs){
                heap.Free(idx);
            }

            allocator.Free(frameResource.allocation);
        }
    }

    void DX12Buffer::upload(
        u32 index,
        const void* src,
        u32 srcSize,
        u32 offset
    ){
        CROWY_ASSERT(srcSize <= GetSize() - offset);

        auto& frameResource = resources[index];

        std::memcpy(
            ptrAdd(frameResource.mapped, offset),
            src,
            srcSize
        );

    #if defined(_DEBUG) || !defined(NDEBUG)
        frameResource.slotWritten = true;
    #endif
    }

    void DX12Buffer::download(
        u32 index,
        void* dst,
        u32 dstSize,
        u32 offset
    ){
        CROWY_ASSERT(dstSize <= GetSize() - offset);

        auto& frameResource = resources[index];

        std::memcpy(
            dst,
            ptrAdd(frameResource.mapped, offset),
            dstSize
        );
    }

    u32 DX12Buffer::GetSize() const noexcept{
        // buffers pad to no more than a few hundred bytes (constant-buffer
        // alignment), so the allocator's byte count fits u32 exactly the
        // same as GetDesc().Width did
        return static_cast<u32>(resources[currentIndex()].allocation.size);
    }

    D3D12_GPU_VIRTUAL_ADDRESS DX12Buffer::GetGPUAddress(){
    #if defined(_DEBUG) || !defined(NDEBUG)
        AssertSlotWritten();
    #endif

        auto& frameResource = resources[currentIndex()];
        return frameResource.allocation.resource->GetGPUVirtualAddress();
    }

    u64 DX12Buffer::GetReadableID(const RHIBufferViewDesc& desc){
    #if defined(_DEBUG) || !defined(NDEBUG)
        AssertSlotWritten();
    #endif

        auto& frameResource = resources[currentIndex()];
        auto& srvs = frameResource.srvs;
        if(auto it = srvs.find(desc); it != srvs.end())
            return it->second;

        const auto sizeBytes = GetSize();
        const auto dxDesc = std::visit(overload{
            [&](const RHIBufferViewDesc::Raw&){
                // raw views address 4-byte words
                return CD3DX12_SHADER_RESOURCE_VIEW_DESC::RawBuffer(
                    sizeBytes / 4
                );
            },
            [&](const RHIBufferViewDesc::Typed& c){
                return CD3DX12_SHADER_RESOURCE_VIEW_DESC::TypedBuffer(
                    convert(c.format),
                    sizeBytes / detail::GetBytesPerPixel(c.format)
                );
            },
            [&](const RHIBufferViewDesc::Structured& c){
                return CD3DX12_SHADER_RESOURCE_VIEW_DESC::StructuredBuffer(
                    sizeBytes / c.stride,
                    c.stride
                );
            }
        }, desc.config);

        auto idx = heap.Allocate(
            *frameResource.allocation.resource,
            dxDesc
        );
        auto [it, ret] = srvs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    u64 DX12Buffer::GetWritableID(const RHIBufferViewDesc& desc){
        auto& frameResource = resources[currentIndex()];
        auto& uavs = frameResource.uavs;
        if(auto it = uavs.find(desc); it != uavs.end())
            return it->second;

        const auto sizeBytes = GetSize();
        const auto dxDesc = std::visit(overload{
            [&](const RHIBufferViewDesc::Raw&){
                // raw views address 4-byte words
                return CD3DX12_UNORDERED_ACCESS_VIEW_DESC::RawBuffer(
                    sizeBytes / 4
                );
            },
            [&](const RHIBufferViewDesc::Typed& c){
                return CD3DX12_UNORDERED_ACCESS_VIEW_DESC::TypedBuffer(
                    convert(c.format),
                    sizeBytes / detail::GetBytesPerPixel(c.format)
                );
            },
            [&](const RHIBufferViewDesc::Structured& c){
                return CD3DX12_UNORDERED_ACCESS_VIEW_DESC::StructuredBuffer(
                    sizeBytes / c.stride,
                    c.stride
                );
            }
        }, desc.config);

        auto idx = heap.Allocate(
            *frameResource.allocation.resource,
            dxDesc
        );
        auto [it, ret] = uavs.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }
}
