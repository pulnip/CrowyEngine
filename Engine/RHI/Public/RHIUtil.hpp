#pragma once

#include <span>
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    void UploadGpuOnlyBuffer(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHIBuffer& buffer,
        // shapes the resting state after the upload (which read uses to cover)
        RHIBufferUsage usage,
        const RHISubresourceData& sub
    );

    void UploadTexture(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHITexture& texture,
        std::span<const RHISubresourceData> subs,
        std::span<const RHISubresourceLayout> layouts,
        const u64 totalBytes
    );

    RHIPixelFormat toSrgb(RHIPixelFormat format);

    // 8-bit 4-channel pixels with rows rowPitch bytes apart; bgra
    // selects the channel order. Writes a 32bpp bottom-up BMP.
    // Returns false when the file cannot be created.
    bool WriteBMP(
        const u8* pixels,
        usize rowPitch,
        u32 width,
        u32 height,
        bool bgra,
        const Str& path
    );
}
