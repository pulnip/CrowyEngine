#pragma once

#include <Foundation/NSString.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLDepthStencil.hpp>
#include <Metal/MTLTexture.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // Metal's buffer argument table has 31 entries (indices 0-30),
    // and slang assigns shader parameter buffers upward from index 0.
    // so, Vertex buffers are bound downward from the table's top
    inline constexpr u32 MaxVertexBufferSlots = 8;

    inline constexpr NS::UInteger toVertexBufferIndex(u32 slot){
        constexpr NS::UInteger top = 30;
        return top - slot;
    }

    NS::String* toNSString(StrView);

    MTL::PixelFormat convert(RHIPixelFormat);
    MTL::CompareFunction convert(RHIComparisonFunc);
    [[nodiscard]]
    MTL::TextureDescriptor* convert(const RHITextureCreateDesc&);

    RHIPixelFormat convert(MTL::PixelFormat);
}
