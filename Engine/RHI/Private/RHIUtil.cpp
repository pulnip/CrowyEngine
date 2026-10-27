#include "RHIUtil.hpp"

#include <array>

#include "PtrUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHITexture.hpp"
#include "UploadRing.hpp"

namespace Crowy
{
    void uploadGPUOnlyBuffer(
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

        // brand-new resource: first use is a self-contained acquire
        const std::array acquires{
            makeBarrier(buffer, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst)
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
        // release stays unmatched here and completes at Close.
        // it runs once per resource, which is why it rests wide open rather
        // than naming the reads a caller happens to have in mind
        const std::array releases{
            RHIBufferBarrier{
                .buffer = &buffer,
                .syncBefore = RHIBarrierSync::Copy,
                .syncAfter = RHIBarrierSync::All,
                .accessBefore = RHIBarrierAccess::CopyDst,
                .accessAfter = RHIBarrierAccess::Common
            }
        };
        cmdList.EndBlitPass({}, releases);
    }

    void uploadTexture(
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
            makeBarrier(texture, RHIResourceUsage::Undefined, RHIResourceUsage::CopyDst)
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
}
