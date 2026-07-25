#pragma once

#include <Foundation/NSString.hpp>
#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLDepthStencil.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    NS::String* toNSString(StrView);

    MTL::PixelFormat convert(RHIPixelFormat);
    MTL::CompareFunction convert(RHIComparisonFunc);

    RHIPixelFormat convert(MTL::PixelFormat);
}
