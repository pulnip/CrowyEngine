#pragma once

#include "ImageLoader.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // RGBA8 rows, top-down, rowPitch bytes apart
    struct Rgba8View {
        const u8* pixels = nullptr;
        u32 width = 0;
        u32 height = 0;
        usize rowPitch = 0;
    };

    // reads blob, never subs: a copied ImageData's subs still point into the
    // source's blob
    Rgba8View viewRgba8(const ImageData& image);
}
