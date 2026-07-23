#pragma once

#include <Metal/MTLPixelFormat.hpp>
#include <Metal/MTLDepthStencil.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MTL::PixelFormat convert(RHIPixelFormat);
    MTL::CompareFunction convert(RHIComparisonFunc);

    RHIPixelFormat convert(MTL::PixelFormat);
}
