#include "RHIUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"
#include "PtrUtil.hpp"
#include "UploadRing.hpp"

namespace Crowy
{
    void UploadGpuOnlyBuffer(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHIBuffer& buffer,
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

        // UNDEFINED to COPY_DST
        cmdList.TransitionBarrier(
            buffer,
            RHIResourceUsage::CopyDst
        );

        cmdList.Copy(
            alloc.buffer,
            buffer,
            alloc.offset,
            0,
            sub.rowPitch
        );

        // TODO.
        // COPY_DST to SHADER_RESOURCE
        cmdList.TransitionBarrier(
            buffer,
            RHIResourceUsage::AnyRead
        );
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

        // UNDEFINED to COPY_DST
        cmdList.TransitionBarrier(
            texture,
            RHIResourceUsage::CopyDst
        );

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

        // COPY_DST to SHADER_RESOURCE
        cmdList.TransitionBarrier(
            texture,
            RHIResourceUsage::AnyRead
        );
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
}
