#pragma once

#include "ImageLoader.hpp"

namespace Crowy
{
    // Replaces a one-level RGBA8 image with its full chain down to 1 x 1, a
    // 2 x 2 box per level (odd edges widen the box); an sRGB image averages
    // in linear light. Throws std::invalid_argument for anything else.
    void generateMipChain(ImageData& image);
}
