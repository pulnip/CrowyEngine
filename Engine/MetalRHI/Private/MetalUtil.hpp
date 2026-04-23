#pragma once

#include <Metal/Metal.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MTL::PixelFormat convertPixelFormat(RHIPixelFormat);
    MTL::CompareFunction convert(RHIComparisonFunc);

}