#pragma once

#include <Metal/MTLCommandBuffer.hpp>
#include <QuartzCore/CAMetalDrawable.hpp>

namespace Crowy
{
    // Debug frame capture for the Metal backend.
    //
    // With CROWY_DUMP_FRAME=<path> set, the Nth presented frame
    // (N = CROWY_DUMP_FRAME_AT, default 60) is written to <path> as a
    // BMP once the GPU finishes it. At most one frame per process.
    //
    // Reading the drawable back requires the swapchain layer to keep
    // framebufferOnly disabled, and an 8-bit RGBA/BGRA drawable format.
    void DumpFrameIfRequested(MTL::CommandBuffer&, CA::MetalDrawable*);
}
