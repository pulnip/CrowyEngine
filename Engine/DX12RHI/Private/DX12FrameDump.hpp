#pragma once

#include "DX12Definitions.hpp"

namespace Crowy
{
    // With CROWY_DUMP_FRAME=<path> set, the Nth presented frame
    // (N = CROWY_DUMP_FRAME_AT, default 60) is written to <path> as a
    // BMP. Call right before IDXGISwapChain::Present, while the back
    // buffer still holds the frame.
    void DumpFrameIfRequested(
        CommandQueue& queue,
        Texture& backBuffer
    );
}
