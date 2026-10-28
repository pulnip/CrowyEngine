#pragma once

#include "DX12Definitions.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // copies the back buffer back, waits, and writes path as a BMP; call
    // right before Present, while the back buffer still holds the frame
    bool DumpFrame(CommandQueue& queue, Texture& backBuffer, const Str& path);
}
