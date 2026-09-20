#pragma once

#include "DX12Definitions.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // Debug frame capture for the DX12 backend: copies the back buffer
    // back, waits for the copy, and writes path as a BMP. Call right
    // before IDXGISwapChain::Present, while the back buffer still holds
    // the frame. Returns whether the file was written.
    bool DumpFrame(
        CommandQueue& queue,
        Texture& backBuffer,
        const Str& path
    );
}
