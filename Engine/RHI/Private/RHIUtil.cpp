#include <array>
#include <cstdio>
#include <cstring>
#include <vector>
#include "EnumUtil.hpp"
#include "RHIUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"
#include "PtrUtil.hpp"
#include "UploadRing.hpp"

namespace Crowy
{
    namespace{
        // resting state after a creation upload: every read use the buffer's
        // usage flags allow. write usages do not shape it - a later writer
        // must acquire with its own edge anyway.
        RHIBarrierPoint restingReadPoint(RHIBufferUsage usage){
            using enum RHIBufferUsage;
            using S = RHIBarrierSync;
            using A = RHIBarrierAccess;

            auto sync = S::None;
            auto access = A::Common;
            const auto add = [&](RHIBufferUsage use, S s, A a){
                if(hasFlag(usage, use)){
                    sync = combine(sync, s);
                    access = combine(access, a);
                }
            };
            add(VertexBuffer,     S::VertexShading,   A::VertexBuffer);
            add(IndexBuffer,      S::IndexInput,      A::IndexBuffer);
            add(ConstantBuffer,   S::AllShading,      A::UniformBuffer);
            add(IndirectArgument, S::ExecuteIndirect, A::IndirectArgs);
            add(ShaderResource,   S::AllShading,      A::ShaderResource);

            // usage flags say nothing about reads (bindless SRV) - stay broad
            if(access == A::Common){
                sync = S::AllShading;
                access = A::ShaderResource;
            }
            return {sync, access, RHITextureLayout::Undefined};
        }
    }

    void UploadGpuOnlyBuffer(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHIBuffer& buffer,
        RHIBufferUsage usage,
        const RHISubresourceData& sub
    ){
        const auto totalBytes = buffer.GetSize();

        // Allocate Staging buffer
        auto alloc = ring.Allocate(
            totalBytes,
            align
        );

        // Copy from CPU to Staging buffer
        alloc.buffer.Upload(
            sub.data,
            sub.rowPitch,
            alloc.offset
        );

        // brand-new resource: first use is a self-contained acquire
        const std::array acquires{
            MakeBarrier(buffer, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst)
        };
        cmdList.BeginBlitPass({}, acquires);

        cmdList.Copy(
            alloc.buffer,
            buffer,
            alloc.offset,
            0,
            sub.rowPitch
        );

        // creation-time uploads are consumed by later submissions, so this
        // release stays unmatched here and completes at Close
        const auto resting = restingReadPoint(usage);
        const std::array releases{
            RHIBufferBarrier{
                .buffer = &buffer,
                .syncBefore = RHIBarrierSync::Copy,
                .syncAfter = resting.sync,
                .accessBefore = RHIBarrierAccess::CopyDst,
                .accessAfter = resting.access
            }
        };
        cmdList.EndBlitPass({}, releases);
    }

    void UploadTexture(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        Crowy::RHITexture& texture,
        std::span<const RHISubresourceData> subs,
        std::span<const RHISubresourceLayout> layouts,
        const u64 totalBytes
    ){
        using namespace Crowy;

        const usize n = subs.size();

        // Allocate Staging buffer
        auto alloc = ring.Allocate(
            totalBytes,
            align
        );

        // Copy from CPU to Staging buffer (with convert pitch & packing)
        for(usize s=0; s<n; ++s){
            const auto& layout = layouts[s];
            // subs[s].rowPitch == RowPitch
            // so, copy whole slice
            for(u32 r=0; r<layout.rowCount; ++r){
                alloc.buffer.Upload(
                    ptrAdd(subs[s].data,
                        subs[s].rowPitch * r
                    ),
                    layout.rowSize,
                    alloc.offset + layout.offset + layout.rowPitch * r
                );
            }
        }

        const auto mipLevels = texture.GetMipLevels();

        // brand-new resource: first use is a self-contained acquire
        const std::array acquires{
            MakeBarrier(texture, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst)
        };
        cmdList.BeginBlitPass(acquires);

        for(usize s=0; s<n; ++s){
            cmdList.Copy(
                alloc.buffer,
                alloc.offset + layouts[s].offset,
                layouts[s].rowPitch,
                texture,
                s % mipLevels,
                s / mipLevels
            );
        }

        // creation-time uploads are consumed by later submissions, so this
        // release stays unmatched here and completes at Close. any-shader
        // read is deliberately broad - the caller does not know the consumer
        const std::array releases{
            RHITextureBarrier{
                .texture = &texture,
                .syncBefore = RHIBarrierSync::Copy,
                .syncAfter = RHIBarrierSync::AllShading,
                .accessBefore = RHIBarrierAccess::CopyDst,
                .accessAfter = RHIBarrierAccess::ShaderResource,
                .layoutBefore = RHITextureLayout::CopyDst,
                .layoutAfter = RHITextureLayout::ShaderResource
            }
        };
        cmdList.EndBlitPass(releases);
    }

    RHIPixelFormat toSrgb(RHIPixelFormat format){
        using enum RHIPixelFormat;

        switch(format){
        case RGBA8_UNORM: return RGBA8_UNORM_SRGB;
        case BGRA8_UNORM: return BGRA8_UNORM_SRGB;
        case BC1_UNORM:   return BC1_UNORM_SRGB;
        case BC2_UNORM:   return BC2_UNORM_SRGB;
        case BC3_UNORM:   return BC3_UNORM_SRGB;
        case BC7_UNORM:   return BC7_UNORM_SRGB;
        default:
            std::unreachable();
        }
    }

    bool WriteBMP(
        const u8* pixels,
        usize rowPitch,
        u32 width,
        u32 height,
        bool bgra,
        const Str& path
    ){
    #ifdef _WIN32
        FILE* file = nullptr;
        if(fopen_s(&file, path.c_str(), "wb") != 0){
            file = nullptr;
        }
    #else
        FILE* file = std::fopen(path.c_str(), "wb");
    #endif
        if(file == nullptr){
            return false;
        }

        // the header is packed by hand because BITMAPFILEHEADER
        // needs 2-byte struct packing
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

        return true;
    }
}
