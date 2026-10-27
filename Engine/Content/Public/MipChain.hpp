#pragma once

#include "ImageLoader.hpp"

namespace Crowy
{
    // one-level RGBA8 to its chain down to 1 x 1: 2 x 2 boxes, an odd edge
    // folded into the last; sRGB averaged in linear light, others throw
    void generateMipChain(ImageData& image);
}
