#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include "DX12FrameDump.hpp"
#include "LogLocal.hpp"

namespace Crowy
{
    namespace{
        struct DumpRequest{
            const char* path = nullptr;
            int frameIndex = 60;
        };

        DumpRequest readRequest(){
            DumpRequest request;
            request.path = std::getenv("CROWY_DUMP_FRAME");

            if(const char* at = std::getenv("CROWY_DUMP_FRAME_AT")){
                if(const int parsed = std::atoi(at); parsed > 0)
                    request.frameIndex = parsed;
            }

            return request;
        }

        // 32bpp bottom-up BMP; the header is packed by hand because
        // BITMAPFILEHEADER needs 2-byte struct packing
        void writeBMP(
            const u8* pixels,
            usize rowPitch,
            u32 width,
            u32 height,
            bool bgra,
            const std::string& path
        ){
            FILE* file = nullptr;
            if(fopen_s(&file, path.c_str(), "wb") != 0 || file == nullptr){
                LOG_WARN(
                    "CROWY_DUMP_FRAME skipped: cannot open '{}'",
                    path
                );
                return;
            }

            u8 header[54] = {};
            const auto put16 = [&](usize at, u16 v){
                header[at] = static_cast<u8>(v & 0xFF);
                header[at + 1] = static_cast<u8>(v >> 8);
            };
            const auto put32 = [&](usize at, u32 v){
                header[at] = static_cast<u8>(v & 0xFF);
                header[at + 1] = static_cast<u8>((v >> 8) & 0xFF);
                header[at + 2] = static_cast<u8>((v >> 16) & 0xFF);
                header[at + 3] = static_cast<u8>((v >> 24) & 0xFF);
            };

            const u32 imageSize = width * height * 4;
            header[0] = 'B'; header[1] = 'M';
            put32(2, 54 + imageSize);   // file size
            put32(10, 54);              // pixel data offset
            put32(14, 40);              // BITMAPINFOHEADER size
            put32(18, width);
            put32(22, height);          // positive height = bottom-up
            put16(26, 1);               // planes
            put16(28, 32);              // bits per pixel
            put32(30, 0);               // BI_RGB
            put32(34, imageSize);
            std::fwrite(header, 1, sizeof(header), file);

            // BMP pixel order is BGRA, bottom row first
            std::vector<u8> row(static_cast<usize>(width) * 4);
            for(u32 y = height; y-- > 0;){
                const u8* src = pixels + y * rowPitch;
                if(bgra){
                    std::memcpy(row.data(), src, row.size());
                }
                else{
                    for(u32 x = 0; x < width; ++x){
                        row[x * 4 + 0] = src[x * 4 + 2];
                        row[x * 4 + 1] = src[x * 4 + 1];
                        row[x * 4 + 2] = src[x * 4 + 0];
                        row[x * 4 + 3] = src[x * 4 + 3];
                    }
                }
                std::fwrite(row.data(), 1, row.size(), file);
            }
            std::fclose(file);

            LOG_INFO(
                "CROWY_DUMP_FRAME: wrote {}x{} frame to '{}'",
                width, height, path
            );
        }
    }

    void DumpFrameIfRequested(
        CommandQueue& queue,
        Texture& backBuffer
    ){
        static const DumpRequest request = readRequest();
        if(request.path == nullptr)
            return;

        static int presentedCount = 0;
        if(++presentedCount != request.frameIndex)
            return;

        const auto desc = backBuffer.GetDesc();
        const bool bgra =
            desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM ||
            desc.Format == DXGI_FORMAT_B8G8R8A8_UNORM_SRGB;
        const bool rgba =
            desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM ||
            desc.Format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        if(!bgra && !rgba){
            LOG_WARN(
                "CROWY_DUMP_FRAME skipped: "
                "back buffer format ({}) is not 8-bit RGBA/BGRA",
                static_cast<u32>(desc.Format)
            );
            return;
        }

        DeviceRAII device;
        if(FAILED(queue.GetDevice(IID_PPV_ARGS(&device)))){
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot query device");
            return;
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
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot create readback buffer");
            return;
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
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot create command list");
            return;
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
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot close command list");
            return;
        }

        ID3D12CommandList* lists[] = { cmdList.Get() };
        queue.ExecuteCommandLists(1, lists);

        FenceRAII fence;
        if(FAILED(device->CreateFence(
            0,
            D3D12_FENCE_FLAG_NONE,
            IID_PPV_ARGS(&fence)
        )) || FAILED(queue.Signal(fence.Get(), 1))){
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot signal fence");
            return;
        }

        HANDLE event = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        if(event == nullptr){
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot create fence event");
            return;
        }
        fence->SetEventOnCompletion(1, event);
        const auto waited = WaitForSingleObject(event, 5000);
        CloseHandle(event);
        if(waited != WAIT_OBJECT_0){
            LOG_WARN("CROWY_DUMP_FRAME skipped: copy did not complete");
            return;
        }

        u8* mapped = nullptr;
        const D3D12_RANGE readRange{0, static_cast<SIZE_T>(totalBytes)};
        if(FAILED(readback->Map(
            0,
            &readRange,
            reinterpret_cast<void**>(&mapped)
        ))){
            LOG_WARN("CROWY_DUMP_FRAME skipped: cannot map readback buffer");
            return;
        }

        writeBMP(
            mapped,
            footprint.Footprint.RowPitch,
            static_cast<u32>(desc.Width),
            desc.Height,
            bgra,
            request.path
        );

        const D3D12_RANGE writtenRange{0, 0};
        readback->Unmap(0, &writtenRange);
    }
}
