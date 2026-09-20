#include "DX12FrameDump.hpp"
#include "LogLocal.hpp"
#include "RHIUtil.hpp"

namespace Crowy
{
    bool DumpFrame(
        CommandQueue& queue,
        Texture& backBuffer,
        const Str& path
    ){
        const auto desc = backBuffer.GetDesc();
        const bool bgra =
            desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        const bool rgba =
            desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
            desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if(!bgra && !rgba){
            LOG_WARN(
                "frame dump skipped: "
                "back buffer format ({}) is not 8-bit RGBA/BGRA",
                static_cast<u32>(desc.Format)
            );
            return false;
        }

        DeviceRAII device;
        if(FAILED(queue.GetDevice(IID_PPV_ARGS(&device)))){
            LOG_WARN("frame dump skipped: cannot query device");
            return false;
        }

        // readback layout of subresource 0 (row pitch is 256-aligned)
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT64 totalBytes = 0;
        device->GetCopyableFootprints(
            &desc,
            0, 1, 0,
            &footprint,
            nullptr,
            nullptr,
            &totalBytes
        );

        const D3D12_HEAP_PROPERTIES heapProps{
            .Type = D3D12_HEAP_TYPE_READBACK
        };
        const D3D12_RESOURCE_DESC readbackDesc{
            .Dimension = D3D12_RESOURCE_DIMENSION_BUFFER,
            .Alignment = 0,
            .Width = totalBytes,
            .Height = 1,
            .DepthOrArraySize = 1,
            .MipLevels = 1,
            .Format = DXGI_FORMAT_UNKNOWN,
            .SampleDesc = {1, 0},
            .Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR,
            .Flags = D3D12_RESOURCE_FLAG_NONE
        };
        BufferRAII readback;
        if(FAILED(device->CreateCommittedResource(
            &heapProps,
            D3D12_HEAP_FLAG_NONE,
            &readbackDesc,
            D3D12_RESOURCE_STATE_COPY_DEST,
            nullptr,
            IID_PPV_ARGS(&readback)
        ))){
            LOG_WARN("frame dump skipped: cannot create readback buffer");
            return false;
        }

        CommandAllocatorRAII allocator;
        CommandListRAII cmdList;
        if(FAILED(device->CreateCommandAllocator(
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            IID_PPV_ARGS(&allocator)
        )) || FAILED(device->CreateCommandList(
            0,
            D3D12_COMMAND_LIST_TYPE_DIRECT,
            allocator.Get(),
            nullptr,
            IID_PPV_ARGS(&cmdList)
        ))){
            LOG_WARN("frame dump skipped: cannot create command list");
            return false;
        }

        // the back buffer sits in PRESENT (= COMMON) layout here, so the
        // copy relies on implicit COMMON -> COPY_SOURCE promotion and the
        // read-only promotion decays back to COMMON after execution;
        // no explicit barrier, which also avoids mixing legacy barriers
        // into an enhanced-barrier codebase
        const D3D12_TEXTURE_COPY_LOCATION src{
            .pResource = &backBuffer,
            .Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX,
            .SubresourceIndex = 0
        };
        const D3D12_TEXTURE_COPY_LOCATION dst{
            .pResource = readback.Get(),
            .Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT,
            .PlacedFootprint = footprint
        };
        cmdList->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);
        if(FAILED(cmdList->Close())){
            LOG_WARN("frame dump skipped: cannot close command list");
            return false;
        }

        ID3D12CommandList* lists[] = { cmdList.Get() };
        queue.ExecuteCommandLists(1, lists);

        FenceRAII fence;
        if(FAILED(device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&fence)
        )) || FAILED(queue.Signal(fence.Get(), 1))){
            LOG_WARN("frame dump skipped: cannot signal fence");
            return false;
        }

        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if(event == nullptr){
            LOG_WARN("frame dump skipped: cannot create fence event");
            return false;
        }
        fence->SetEventOnCompletion(1, event);
        const auto waited = WaitForSingleObject(event, 5000);
        CloseHandle(event);
        if(waited != WAIT_OBJECT_0){
            LOG_WARN("frame dump skipped: copy did not complete");
            return false;
        }

        u8* mapped = nullptr;
        const D3D12_RANGE readRange{0, static_cast<SIZE_T>(totalBytes)};
        if(FAILED(readback->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&mapped)
        ))){
            LOG_WARN("frame dump skipped: cannot map readback buffer");
            return false;
        }

        const auto width = static_cast<u32>(desc.Width);
        const bool written = WriteBMP(
            mapped,
            footprint.Footprint.RowPitch,
            width,
            desc.Height,
            bgra,
            path
        );
        if(!written){
            LOG_WARN("frame dump skipped: cannot open '{}'", path);
        }
        else{
            LOG_INFO(
                "frame dump: wrote {}x{} frame to '{}'",
                width, desc.Height, path
            );
        }

        const D3D12_RANGE writtenRange{0, 0};
        readback->Unmap(0, &writtenRange);

        return written;
    }
}
