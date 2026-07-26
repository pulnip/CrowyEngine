#pragma once

#include <Foundation/NSString.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLDepthStencil.hpp>
#include <Metal/MTLTexture.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    NS::String* toNSString(StrView);

    MTL::PixelFormat convert(RHIPixelFormat);
    MTL::CompareFunction convert(RHIComparisonFunc);
    [[nodiscard]]
    MTL::TextureDescriptor* convert(const RHITextureCreateDesc&);

    RHIPixelFormat convert(MTL::PixelFormat);
}
