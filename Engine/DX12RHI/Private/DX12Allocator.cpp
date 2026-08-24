#include <d3dx12/d3dx12_core.h>
#include "Assert.hpp"
#include "DX12Allocator.hpp"
#include "DX12Util.hpp"

namespace{
    D3D12_HEAP_TYPE toHeapType(
        Crowy::RHIMemoryType memory,
        const Crowy::DX12Capabilities& capabilities
    ){
        using enum Crowy::RHIMemoryType;

        switch(memory){
        case GPUOnly:
            [[fallthrough]];
        // D3D12 has no tile memory to keep it in
        case Transient:
            return D3D12_HEAP_TYPE_DEFAULT;
        case CPUWrite:
            // ReBAR turns the upload heap into device-local memory the CPU
            // can still write, which is strictly better where it exists
            return capabilities.gpuUploadHeap ?
                D3D12_HEAP_TYPE_GPU_UPLOAD : D3D12_HEAP_TYPE_UPLOAD;
        case CPURead:
            return D3D12_HEAP_TYPE_READBACK;
        default:
            std::unreachable();
        }
    }
}

namespace Crowy
{
    DX12Allocator::DX12Allocator(
        Device& device,
        const DX12Capabilities& capabilities
    )
        : device(device)
        , capabilities(capabilities){}

    DX12Allocator::~DX12Allocator(){
        CROWY_ASSERT(live.empty(),
            "{} allocations outlived the allocator",
            live.size()
        );
    }

    RHIAllocation DX12Allocator::Allocate(
        const D3D12_RESOURCE_DESC1& desc,
        RHIMemoryType memory,
        const D3D12_CLEAR_VALUE* clearValue,
        StrView name
    ){
        const auto heapProp = CD3DX12_HEAP_PROPERTIES(
            ::toHeapType(memory, capabilities)
        );

        COMRAII<Buffer> resource;
        CHECK_HRESULT(device.CreateCommittedResource3(
            &heapProp,
            D3D12_HEAP_FLAG_NONE,
            &desc,
            // nothing has been written yet, whichever kind this is
            D3D12_BARRIER_LAYOUT_UNDEFINED,
            clearValue,
            // Hardware DRM
            nullptr,
            // Relaxed Format Casting
            0, nullptr,
            IID_PPV_ARGS(&resource)
        ), "Failed to allocate DX12 resource");

    #if defined(_DEBUG) || !defined(NDEBUG)
        if(!name.empty()){
            resource->SetPrivateData(
                WKPDID_D3DDebugObjectName,
                static_cast<UINT>(name.length()),
                name.data()
            );
        }
    #endif

        const RHIAllocation allocation{
            .resource = resource.Get(),
            .heapOffset = 0,
            // Width is already the byte count for a buffer; for anything
            // else only the driver knows what the layout costs
            .size = desc.Dimension == D3D12_RESOURCE_DIMENSION_BUFFER ?
                desc.Width :
                device.GetResourceAllocationInfo2(0, 1, &desc, nullptr).SizeInBytes
        };

        const auto [_, inserted] = live.emplace(
            allocation.resource,
            std::move(resource)
        );
        CROWY_ASSERT(inserted);

        return allocation;
    }

    void DX12Allocator::Free(const RHIAllocation& allocation){
        if(allocation.resource == nullptr)
            return;

        const auto erased = live.erase(allocation.resource);
        CROWY_ASSERT(erased == 1,
            "freed an allocation this allocator never handed out"
        );
    }
}
