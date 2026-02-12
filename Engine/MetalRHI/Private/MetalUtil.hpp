#pragma once

#include <Metal/Metal.hpp>
#include "RHIDefinitions.hpp"

namespace Crowy
{
    MTL::PixelFormat convertTextureFormat(RHITextureFormat);
    MTL::CompareFunction convert(RHIComparisonFunc);

}