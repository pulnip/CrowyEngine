#pragma once

#include <functional>
#include <Metal/MTLCommandBuffer.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>
#include "Primitives.hpp"

namespace Crowy
{
    // Debug frame capture for the Metal backend: writes the drawable to
    // path as a BMP once the GPU finishes the command buffer, then reports
    // through onDone - from the completion handler's thread.
    //
    // Reading the drawable back requires the swapchain layer to keep
    // framebufferOnly disabled, and an 8-bit RGBA/BGRA drawable format.
    void DumpFrame(
        MTL::CommandBuffer&,
        CA::MetalDrawable&,
        Str path,
        std::function<void(bool written)> onDone
    );
}
