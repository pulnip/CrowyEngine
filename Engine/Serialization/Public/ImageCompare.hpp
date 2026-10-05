#pragma once

#include <array>
#include <limits>
#include <vector>

#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // One backend repeats its frames byte for byte, so by default no pixel
    // may fail; a looser comparison, such as across backends, passes fractions.
    struct ImageTolerance {
        // per channel, absolute, inclusive
        u8 channelDelta = 2;
        // failing pixels over all pixels
        f64 maxGlobalFail = 0.0;
        // failing pixels over the pixels of the worst tile in a 10 x 10 grid
        f64 maxLocalFail = 0.0;
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

    enum class EdgeState : u8 {
        None,
        Unmatched,
        Matched,
    };

    // Two renderers place things alike when their edges coincide, whatever
    // the tone of either picture.
    struct EdgeOptions {
        // chessboard pixels within which one picture's edge finds the other's
        u32 radius = 2;
        // the thinned edges each picture keeps, strongest first, over its
        // pixels
        f64 density = 0.05;
        // the side searched keeps this many times more, so a tone curve that
        // reorders edge strengths does not read as a missing edge
        f64 lenience = 2.0;
        // the largest offset per axis the shift search tries
        u32 maxShift = 8;
        // a tile with fewer of A's edges is not ranked
        u32 minTileEdges = 100;
    };

    struct EdgeComparison {
        EdgeOptions options;
        u32 width = 0;
        u32 height = 0;
        u64 edgesA = 0;
        u64 edgesB = 0;
        // A's edges with one of B's within the radius, over A's edges, and
        // the reverse; 1 without edges
        f64 aNearB = 1.0;
        f64 bNearA = 1.0;
        // where A's edges find B's least; worstTileEdges 0 when no tile ranks
        u32 tileX = 0;
        u32 tileY = 0;
        TileRect worstTile;
        u64 worstTileEdges = 0;
        f64 worstTileNear = 1.0;
        // where B's content sits against A's, by exact coincidence both ways;
        // a tie keeps the shift nearest (0, 0)
        i32 shiftX = 0;
        i32 shiftY = 0;
        f64 shiftCoincide = 0.0;
        f64 zeroCoincide = 0.0;
        // per pixel, rows top-down
        std::vector<EdgeState> a;
        std::vector<EdgeState> b;
    };

    // 1 where `image` has a thinned edge among its strongest `density` of
    // pixels, rows top-down
    std::vector<u8> findEdges(Rgba8View image, f64 density);
    // throws std::invalid_argument unless both hold pixels of one non-empty
    // size
    EdgeComparison compareEdges(
        Rgba8View a,
        Rgba8View b,
        const EdgeOptions& options = {}
    );
    // RGBA8, rows top-down: A's luma dimmed, matched edges grey, A's alone
    // magenta, B's alone green, the worst tile outlined in yellow
    std::vector<u8> paintEdgeOverlay(
        const EdgeComparison& comparison,
        Rgba8View a
    );
}
