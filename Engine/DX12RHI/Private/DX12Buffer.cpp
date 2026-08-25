#include <d3dx12/d3dx12_core.h>
#include "Assert.hpp"
#include "DescriptorHeapAllocator.hpp"
#include "DX12Buffer.hpp"
#include "DX12Definitions.hpp"
#include "DX12Util.hpp"
#include "IntMath.hpp"
#include "PtrUtil.hpp"
#include "VariantUtil.hpp"

namespace{
    // the element range a view covers, in whatever unit that view addresses
    struct ViewRange{
        UINT64 firstElement;
        UINT numElements;
    };

    ViewRange resolveRange(
        const Crowy::RHIBufferViewDesc& desc,
        Crowy::u32 bufferSize,
        Crowy::u32 elementSize
    ){
        CROWY_ASSERT(elementSize > 0);
        CROWY_ASSERT(desc.offset % elementSize == 0,
            "buffer view offset {} does not land on a {}-byte element",
            desc.offset, elementSize
        );

        const auto bytes = desc.size != 0 ? desc.size : bufferSize - desc.offset;
        CROWY_ASSERT(desc.offset + bytes <= bufferSize,
            "buffer view [{}, {}) runs past the {}-byte buffer",
            desc.offset, desc.offset + bytes, bufferSize
        );

        return ViewRange{
            .firstElement = desc.offset / elementSize,
            .numElements = bytes / elementSize
        };
    }
}

namespace Crowy
{
    DX12Buffer::DX12Buffer(
        DX12Allocator& allocator,
        const RHIBufferCreateDesc& desc,
        DescriptorHeapAllocator& heap,
        StrView name
    )
        : size(desc.size)
        , allocator(allocator)
        , heap(heap)
    {
        using enum RHIMemoryType;

        CROWY_ASSERT(desc.memory != Transient,
            "tile memory holds render targets, not buffers"
        );
        CROWY_ASSERT(desc.memory == GPUOnly || !desc.shaderWrite,
            "a shader cannot write CPU-visible memory"
        );

        // every buffer is rounded to the constant-buffer placement, so any of
        // them can back a CBV; the heap granularity swallows the padding
        const auto bufDesc = CD3DX12_RESOURCE_DESC1::Buffer(
            D3D12_RESOURCE_ALLOCATION_INFO{
                .SizeInBytes = nextMul<u32>(
                    desc.size,
                    D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT
                ),
                .Alignment = 0
            },
            desc.shaderWrite ?
                D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS :
                D3D12_RESOURCE_FLAG_NONE
        );

        allocation = allocator.Allocate(
            bufDesc,
            desc.memory,
            nullptr,
            name
        );

        if(desc.memory != GPUOnly){
            const CD3DX12_RANGE noRead(0, 0);
            CHECK_HRESULT(allocation.resource->Map(
                0,
                &noRead,
                &mapped
            ), "Failed to Map DX12 Buffer");
        }

        if(desc.initialData != nullptr && desc.memory == CPUWrite){
            Upload(desc.initialData, desc.size);
        }
    }

    DX12Buffer::~DX12Buffer(){
        if(mapped != nullptr){
            allocation.resource->Unmap(0, nullptr);
            mapped = nullptr;
        }

        for(const auto& [_, idx]: cbvs){
            heap.Free(idx);
        }
        for(const auto& [_, idx]: srvs){
            heap.Free(idx);
        }
        for(const auto& [_, idx]: uavs){
            heap.Free(idx);
        }

        allocator.Free(allocation);
    }

    void DX12Buffer::Upload(
        const void* src,
        u32 srcSize,
        u32 offset
    ){
        CROWY_ASSERT(mapped != nullptr, "buffer is not CPU-writable");
        CROWY_ASSERT(srcSize <= size - offset);

        std::memcpy(
            ptrAdd(mapped, offset),
            src,
            srcSize
        );
    }

    void DX12Buffer::Download(
        void* dst,
        u32 dstSize,
        u32 offset
    ){
        CROWY_ASSERT(mapped != nullptr, "buffer is not CPU-readable");
        CROWY_ASSERT(dstSize <= size - offset);

        std::memcpy(
            dst,
            ptrAdd(mapped, offset),
            dstSize
        );
    }

    UINT DX12Buffer::createView(
        const RHIBufferViewDesc& desc,
        std::unordered_map<RHIBufferViewDesc, UINT>& cache,
        bool writable
    ){
        if(auto it = cache.find(desc); it != cache.end())
            return it->second;

        const auto idx = std::visit(overload{
            [&](const RHIBufferViewDesc::Raw&){
                // raw views address 4-byte words
                const auto range = ::resolveRange(desc, size, 4);
                return writable ?
                    heap.Allocate(
                        *allocation.resource,
                        CD3DX12_UNORDERED_ACCESS_VIEW_DESC::RawBuffer(
                            range.numElements, range.firstElement
                        )
                    ) :
                    heap.Allocate(
                        *allocation.resource,
                        CD3DX12_SHADER_RESOURCE_VIEW_DESC::RawBuffer(
                            range.numElements, range.firstElement
                        )
                    );
            },
            [&](const RHIBufferViewDesc::Typed& c){
                const auto range = ::resolveRange(
                    desc, size, detail::GetBytesPerPixel(c.format)
                );
                return writable ?
                    heap.Allocate(
                        *allocation.resource,
                        CD3DX12_UNORDERED_ACCESS_VIEW_DESC::TypedBuffer(
                            convert(c.format),
                            range.numElements, range.firstElement
                        )
                    ) :
                    heap.Allocate(
                        *allocation.resource,
                        CD3DX12_SHADER_RESOURCE_VIEW_DESC::TypedBuffer(
                            convert(c.format),
                            range.numElements, range.firstElement
                        )
                    );
            },
            [&](const RHIBufferViewDesc::Structured& c){
                const auto range = ::resolveRange(desc, size, c.stride);
                return writable ?
                    heap.Allocate(
                        *allocation.resource,
                        CD3DX12_UNORDERED_ACCESS_VIEW_DESC::StructuredBuffer(
                            range.numElements, c.stride, range.firstElement
                        )
                    ) :
                    heap.Allocate(
                        *allocation.resource,
                        CD3DX12_SHADER_RESOURCE_VIEW_DESC::StructuredBuffer(
                            range.numElements, c.stride, range.firstElement
                        )
                    );
            }
        }, desc.config);

        auto [it, ret] = cache.emplace(desc, idx);
        CROWY_ASSERT(ret);

        return idx;
    }

    u64 DX12Buffer::GetReadableID(const RHIBufferViewDesc& desc){
        return createView(desc, srvs, false);
    }

    u64 DX12Buffer::GetWritableID(const RHIBufferViewDesc& desc){
        return createView(desc, uavs, true);
    }
}
