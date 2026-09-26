#include "ImageCompare.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <format>
#include <stdexcept>

namespace
{
    constexpr Crowy::u32 GridTiles = 10;

    // where tile t starts along an axis of size pixels; tiles differ by at
    // most one pixel
    constexpr Crowy::u32 tileEdge(Crowy::u32 size, Crowy::u32 t) {
        return static_cast<Crowy::u32>(
            static_cast<Crowy::u64>(size) * t / GridTiles
        );
    }

    void requireComparable(Crowy::Rgba8View a, Crowy::Rgba8View b) {
        if(a.pixels == nullptr || b.pixels == nullptr)
            throw std::invalid_argument("an image without pixels");

        if(a.width == 0 || a.height == 0 || b.width == 0 || b.height == 0) {
            throw std::invalid_argument(
                std::format(
                    "an empty image ({}x{} against {}x{})",
                    a.width,
                    a.height,
                    b.width,
                    b.height
                )
            );
        }

        if(a.width != b.width || a.height != b.height) {
            throw std::invalid_argument(
                std::format(
                    "sizes differ: {}x{} against {}x{}",
                    a.width,
                    a.height,
                    b.width,
                    b.height
                )
            );
        }

        const auto packed = static_cast<Crowy::usize>(a.width) * 4;
        if(a.rowPitch < packed || b.rowPitch < packed) {
            throw std::invalid_argument(
                std::format(
                    "a row pitch below {} bytes cannot hold {} pixels",
                    packed,
                    a.width
                )
            );
        }
    }

    // the worst tile's failing fraction, each tile over its own pixel count
    void findWorstTile(Crowy::ImageComparison& result) {
        using namespace Crowy;

        const auto limit = result.tolerance.channelDelta;
        const auto fails = [limit](u8 delta) {
            return delta > limit;
        };

        bool found = false;
        for(u32 ty = 0; ty < GridTiles; ++ty) {
            const auto y0 = tileEdge(result.height, ty);
            const auto y1 = tileEdge(result.height, ty + 1);
            for(u32 tx = 0; tx < GridTiles; ++tx) {
                const auto x0 = tileEdge(result.width, tx);
                const auto x1 = tileEdge(result.width, tx + 1);
                // a side under ten pixels leaves some tiles empty
                if(x0 == x1 || y0 == y1)
                    continue;

                u64 failing = 0;
                for(auto y = y0; y < y1; ++y) {
                    const auto* row = result.delta.data() +
                                      static_cast<usize>(y) * result.width;
                    failing += static_cast<u64>(
                        std::count_if(row + x0, row + x1, fails)
                    );
                }

                const auto pixels = static_cast<u64>(x1 - x0) * (y1 - y0);
                const auto fraction =
                    static_cast<f64>(failing) / static_cast<f64>(pixels);
                if(found && fraction <= result.localFail)
                    continue;

                found = true;
                result.localFail = fraction;
                result.tileX = tx;
                result.tileY = ty;
                result.worstTile =
                    TileRect{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
            }
        }
    }
}

namespace Crowy
{
    ImageComparison compareImages(
        Rgba8View a,
        Rgba8View b,
        const ImageTolerance& tolerance
    ) {
        requireComparable(a, b);

        const auto pixelCount = static_cast<u64>(a.width) * a.height;
        const usize channels = tolerance.compareAlpha ? 4 : 3;

        ImageComparison result{
            .tolerance = tolerance,
            .width = a.width,
            .height = a.height
        };
        result.delta.resize(pixelCount);

        std::array<u64, 4> sums{};
        std::array<u64, 4> squares{};
        for(u32 y = 0; y < a.height; ++y) {
            const auto* rowA = a.pixels + y * a.rowPitch;
            const auto* rowB = b.pixels + y * b.rowPitch;
            auto* deltaRow =
                result.delta.data() + static_cast<usize>(y) * a.width;
            for(u32 x = 0; x < a.width; ++x) {
                u8 largest = 0;
                for(usize c = 0; c < channels; ++c) {
                    const auto at = static_cast<usize>(x) * 4 + c;
                    const auto d = static_cast<u8>(
                        std::abs(static_cast<int>(rowA[at]) - rowB[at])
                    );
                    sums[c] += d;
                    squares[c] += static_cast<u64>(d) * d;
                    result.channel[c].max = std::max(result.channel[c].max, d);
                    largest = std::max(largest, d);
                }

                deltaRow[x] = largest;
                if(largest > tolerance.channelDelta)
                    ++result.failing;
            }
        }

        const auto count = static_cast<f64>(pixelCount);
        for(usize c = 0; c < channels; ++c) {
            auto& stats = result.channel[c];
            stats.mean = static_cast<f64>(sums[c]) / count;
            stats.rms = std::sqrt(static_cast<f64>(squares[c]) / count);
            if(stats.rms > 0.0)
                stats.psnr = 20.0 * std::log10(255.0 / stats.rms);
        }

        result.globalFail = static_cast<f64>(result.failing) / count;
        findWorstTile(result);
        result.similar = result.globalFail <= tolerance.maxGlobalFail &&
                         result.localFail <= tolerance.maxLocalFail;

        return result;
    }

    ImageComparison compareImages(
        const ImageData& a,
        const ImageData& b,
        const ImageTolerance& tolerance
    ) {
        return compareImages(viewRgba8(a), viewRgba8(b), tolerance);
    }

    std::vector<u8> paintHeatMap(const ImageComparison& comparison) {
        const int tolerance = comparison.tolerance.channelDelta;
        // the deltas a failing pixel can have past the first, tolerance + 1
        const int failRange = 254 - tolerance;

        std::vector<u8> pixels(comparison.delta.size() * 4);
        for(usize i = 0; i < comparison.delta.size(); ++i) {
            const int d = comparison.delta[i];
            auto* pixel = pixels.data() + i * 4;
            if(d > tolerance) {
                const auto green =
                    failRange > 0
                        ? (255 * (d - tolerance - 1) + failRange / 2) /
                              failRange
                        : 0;
                pixel[0] = 255;
                pixel[1] = static_cast<u8>(green);
            } else if(d > 0) {
                // the floor of 64 keeps a difference apart from black
                const auto grey = 64 + (191 * d + tolerance / 2) / tolerance;
                pixel[0] = pixel[1] = pixel[2] = static_cast<u8>(grey);
            }
            pixel[3] = 255;
        }

        return pixels;
    }
}
