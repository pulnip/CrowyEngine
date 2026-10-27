#pragma once

#include <span>
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    void uploadGPUOnlyBuffer(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHIBuffer& buffer,
        const RHISubresourceData& sub
    );

    void uploadTexture(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHITexture& texture,
        std::span<const RHISubresourceData> subs,
        std::span<const RHISubresourceLayout> layouts,
        const u64 totalBytes
    );

    RHIPixelFormat toSrgb(RHIPixelFormat format);
}
