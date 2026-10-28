#pragma once

#include <functional>

#include <Metal/MTLCommandBuffer.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

#include "Primitives.hpp"

namespace Crowy
{
    // writes the 8-bit RGBA/BGRA drawable to path as a BMP once the GPU is
    // done, then reports through onDone from the completion handler's thread
    void DumpFrame(
        MTL::CommandBuffer&,
        CA::MetalDrawable&,
        Str path,
        std::function<void(bool written)> onDone
    );
}
