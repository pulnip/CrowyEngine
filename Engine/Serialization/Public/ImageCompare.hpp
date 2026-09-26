#pragma once

#include <array>
#include <limits>
#include <vector>

#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct ImageTolerance {
        // per channel, absolute, inclusive
        u8 channelDelta = 2;
        // failing pixels over all pixels
        f64 maxGlobalFail = 0.001;
        // failing pixels over the pixels of the worst tile in a 10 x 10 grid
        f64 maxLocalFail = 0.01;
        bool compareAlpha = false;
    };

    struct ChannelStats {
        u8 max = 0;
        f64 mean = 0.0;
        f64 rms = 0.0;
        // dB, infinite when the channel matches exactly
        f64 psnr = std::numeric_limits<f64>::infinity();
    };

    // pixels [x0, x1) x [y0, y1)
    struct TileRect {
        u32 x0 = 0;
        u32 y0 = 0;
        u32 x1 = 0;
        u32 y1 = 0;
    };

    struct ImageComparison {
        ImageTolerance tolerance;
        u32 width = 0;
        u32 height = 0;
        u64 failing = 0;
        f64 globalFail = 0.0;
        f64 localFail = 0.0;
        // the worst tile's place in the 10 x 10 grid, and its pixels
        u32 tileX = 0;
        u32 tileY = 0;
        TileRect worstTile;
        // R, G, B, A; A stays zero unless compared
        std::array<ChannelStats, 4> channel{};
        // per pixel, the largest compared channel difference, rows top-down
        std::vector<u8> delta;
        bool similar = false;
    };

    // throws std::invalid_argument unless both hold pixels of one non-empty
    // size
    ImageComparison compareImages(
        Rgba8View a,
        Rgba8View b,
        const ImageTolerance& tolerance = {}
    );
    ImageComparison compareImages(
        const ImageData& a,
        const ImageData& b,
        const ImageTolerance& tolerance = {}
    );

    // RGBA8, rows top-down: black where equal, grey within the tolerance, red
    // rising to yellow where a pixel fails
    std::vector<u8> paintHeatMap(const ImageComparison& comparison);
}
