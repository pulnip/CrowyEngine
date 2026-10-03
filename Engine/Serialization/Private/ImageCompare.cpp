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

    // a four-level step: 8-bit banding stays below it
    constexpr Crowy::u32 MinEdgeMagnitude = 16;
    // |Gx| + |Gy| of a 3 x 3 Sobel over 8-bit luma
    constexpr Crowy::u32 MaxEdgeMagnitude = 2040;

    // Rec. 709 weights on the stored bytes
    Crowy::u8 lumaOf(const Crowy::u8* pixel) {
        return static_cast<Crowy::u8>(
            (54u * pixel[0] + 183u * pixel[1] + 19u * pixel[2] + 128u) >> 8
        );
    }

    // per pixel, the Sobel magnitude where non-maximum suppression across the
    // edge keeps it, else 0; a tie goes to the later pixel, whatever the
    // polarity, so two renderers thin one boundary to the same side
    std::vector<Crowy::u16> thinnedEdges(Crowy::Rgba8View image) {
        using namespace Crowy;

        const auto width = static_cast<i64>(image.width);
        const auto height = static_cast<i64>(image.height);
        const auto count = static_cast<usize>(width * height);
        std::vector<u8> luma(count);
        for(i64 y = 0; y < height; ++y) {
            const auto* row =
                image.pixels + static_cast<usize>(y) * image.rowPitch;
            for(i64 x = 0; x < width; ++x)
                luma[static_cast<usize>(y * width + x)] = lumaOf(row + x * 4);
        }

        const auto clamped = [&](i64 x, i64 y) {
            x = std::clamp<i64>(x, 0, width - 1);
            y = std::clamp<i64>(y, 0, height - 1);
            return static_cast<int>(luma[static_cast<usize>(y * width + x)]);
        };
        // the neighbor after a pixel across its edge; the one before mirrors it
        constexpr std::array<std::array<int, 2>, 4> Across = {
            {{1, 0}, {0, 1}, {1, 1}, {-1, 1}}
        };
        std::vector<u16> magnitude(count);
        std::vector<u8> direction(count);
        for(i64 y = 0; y < height; ++y) {
            for(i64 x = 0; x < width; ++x) {
                const auto gx = clamped(x + 1, y - 1) +
                                2 * clamped(x + 1, y) + clamped(x + 1, y + 1) -
                                clamped(x - 1, y - 1) -
                                2 * clamped(x - 1, y) - clamped(x - 1, y + 1);
                const auto gy = clamped(x - 1, y + 1) +
                                2 * clamped(x, y + 1) + clamped(x + 1, y + 1) -
                                clamped(x - 1, y - 1) -
                                2 * clamped(x, y - 1) - clamped(x + 1, y - 1);
                const auto ax = std::abs(gx);
                const auto ay = std::abs(gy);
                const auto at = static_cast<usize>(y * width + x);
                magnitude[at] = static_cast<u16>(ax + ay);
                if(5 * ay <= 2 * ax)
                    direction[at] = 0;
                else if(5 * ax <= 2 * ay)
                    direction[at] = 1;
                else
                    direction[at] = gx * gy > 0 ? 2 : 3;
            }
        }

        const auto magnitudeAt = [&](i64 x, i64 y) -> u32 {
            if(x < 0 || y < 0 || x >= width || y >= height)
                return 0;
            return magnitude[static_cast<usize>(y * width + x)];
        };
        std::vector<u16> kept(count);
        for(i64 y = 0; y < height; ++y) {
            for(i64 x = 0; x < width; ++x) {
                const auto at = static_cast<usize>(y * width + x);
                const u32 value = magnitude[at];
                if(value < MinEdgeMagnitude)
                    continue;

                const auto [dx, dy] = Across[direction[at]];
                if(value >= magnitudeAt(x - dx, y - dy) &&
                   value > magnitudeAt(x + dx, y + dy))
                    kept[at] = magnitude[at];
            }
        }

        return kept;
    }

    // 1 where a thinned edge is among the strongest `density` of all pixels;
    // ties at the threshold are kept
    std::vector<Crowy::u8> keepStrongest(
        const std::vector<Crowy::u16>& magnitudes,
        Crowy::f64 density
    ) {
        using namespace Crowy;

        const auto wanted = static_cast<u64>(
            std::ceil(density * static_cast<f64>(magnitudes.size()))
        );
        std::vector<u64> histogram(MaxEdgeMagnitude + 1);
        for(const auto m: magnitudes)
            ++histogram[m];

        auto threshold = MinEdgeMagnitude;
        u64 stronger = 0;
        for(auto m = MaxEdgeMagnitude; m >= MinEdgeMagnitude; --m) {
            stronger += histogram[m];
            if(stronger >= wanted) {
                threshold = m;
                break;
            }
        }

        std::vector<u8> mask(magnitudes.size());
        for(usize i = 0; i < magnitudes.size(); ++i)
            mask[i] = magnitudes[i] >= threshold ? 1 : 0;

        return mask;
    }

    // a (2 radius + 1)-pixel square maximum, rows then columns
    std::vector<Crowy::u8> dilate(
        const std::vector<Crowy::u8>& mask,
        Crowy::u32 width,
        Crowy::u32 height,
        Crowy::u32 radius
    ) {
        using namespace Crowy;

        // the window's count slides along a line of `size` cells `stride` apart
        const auto slide = [radius](
                               const u8* in,
                               u8* out,
                               u32 size,
                               usize stride
                           ) {
            u32 inside = 0;
            for(u32 i = 0; i <= radius && i < size; ++i)
                inside += in[i * stride];
            for(u32 i = 0; i < size; ++i) {
                out[i * stride] = inside > 0 ? 1 : 0;
                if(i >= radius)
                    inside -= in[(i - radius) * stride];
                if(i + radius + 1 < size)
                    inside += in[(i + radius + 1) * stride];
            }
        };

        std::vector<u8> rows(mask.size());
        for(u32 y = 0; y < height; ++y) {
            const auto at = static_cast<usize>(y) * width;
            slide(mask.data() + at, rows.data() + at, width, 1);
        }
        std::vector<u8> square(mask.size());
        for(u32 x = 0; x < width; ++x)
            slide(rows.data() + x, square.data() + x, height, width);

        return square;
    }

    // the tile of the 10 x 10 grid where A's edges find B's least
    void rankEdgeTiles(Crowy::EdgeComparison& result) {
        using namespace Crowy;

        bool found = false;
        for(u32 ty = 0; ty < GridTiles; ++ty) {
            const auto y0 = tileEdge(result.height, ty);
            const auto y1 = tileEdge(result.height, ty + 1);
            for(u32 tx = 0; tx < GridTiles; ++tx) {
                const auto x0 = tileEdge(result.width, tx);
                const auto x1 = tileEdge(result.width, tx + 1);
                u64 edges = 0;
                u64 matched = 0;
                for(auto y = y0; y < y1; ++y) {
                    for(auto x = x0; x < x1; ++x) {
                        const auto state =
                            result.a[static_cast<usize>(y) * result.width + x];
                        edges += state != EdgeState::None ? 1 : 0;
                        matched += state == EdgeState::Matched ? 1 : 0;
                    }
                }
                if(edges == 0 || edges < result.options.minTileEdges)
                    continue;

                const auto near =
                    static_cast<f64>(matched) / static_cast<f64>(edges);
                if(found && near >= result.worstTileNear)
                    continue;

                found = true;
                result.tileX = tx;
                result.tileY = ty;
                result.worstTile =
                    TileRect{.x0 = x0, .y0 = y0, .x1 = x1, .y1 = y1};
                result.worstTileEdges = edges;
                result.worstTileNear = near;
            }
        }
    }

    // the offset at which each picture's edges land exactly on the other's
    // most often
    void findEdgeShift(
        Crowy::EdgeComparison& result,
        const std::vector<Crowy::u8>& lenientA,
        const std::vector<Crowy::u8>& lenientB
    ) {
        using namespace Crowy;

        const auto width = static_cast<i64>(result.width);
        const auto height = static_cast<i64>(result.height);
        std::vector<std::array<i64, 2>> edgesA;
        std::vector<std::array<i64, 2>> edgesB;
        for(i64 y = 0; y < height; ++y) {
            for(i64 x = 0; x < width; ++x) {
                const auto at = static_cast<usize>(y * width + x);
                if(result.a[at] != EdgeState::None)
                    edgesA.push_back({x, y});
                if(result.b[at] != EdgeState::None)
                    edgesB.push_back({x, y});
            }
        }
        const auto total = edgesA.size() + edgesB.size();
        if(total == 0)
            return;

        // how many of `edges`, moved by (dx, dy), land on `mask`
        const auto landing = [&](const std::vector<std::array<i64, 2>>& edges,
                                 const std::vector<u8>& mask,
                                 i64 dx,
                                 i64 dy) {
            u64 count = 0;
            for(const auto& [x, y]: edges) {
                const auto mx = x + dx;
                const auto my = y + dy;
                if(mx >= 0 && my >= 0 && mx < width && my < height)
                    count += mask[static_cast<usize>(my * width + mx)];
            }
            return count;
        };

        const auto shift = static_cast<i64>(result.options.maxShift);
        u64 best = 0;
        i64 bestDistance = 0;
        bool found = false;
        for(auto dy = -shift; dy <= shift; ++dy) {
            for(auto dx = -shift; dx <= shift; ++dx) {
                const auto count = landing(edgesA, lenientB, dx, dy) +
                                   landing(edgesB, lenientA, -dx, -dy);
                const auto coincide =
                    static_cast<f64>(count) / static_cast<f64>(total);
                if(dx == 0 && dy == 0)
                    result.zeroCoincide = coincide;

                const auto distance = dx * dx + dy * dy;
                if(found && (count < best ||
                             (count == best && distance >= bestDistance)))
                    continue;

                found = true;
                best = count;
                bestDistance = distance;
                result.shiftX = static_cast<i32>(dx);
                result.shiftY = static_cast<i32>(dy);
                result.shiftCoincide = coincide;
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

    std::vector<u8> findEdges(Rgba8View image, f64 density) {
        requireComparable(image, image);
        if(!(density > 0.0 && density <= 1.0)) {
            throw std::invalid_argument(
                std::format("an edge density of {} is not in (0, 1]", density)
            );
        }

        return keepStrongest(thinnedEdges(image), density);
    }

    EdgeComparison compareEdges(
        Rgba8View a,
        Rgba8View b,
        const EdgeOptions& options
    ) {
        requireComparable(a, b);
        if(!(options.density > 0.0 && options.density <= 1.0) ||
           !(options.lenience >= 1.0)) {
            throw std::invalid_argument(
                std::format(
                    "an edge density of {} is not in (0, 1], or a lenience of "
                    "{} is below 1",
                    options.density,
                    options.lenience
                )
            );
        }

        const auto magnitudesA = thinnedEdges(a);
        const auto magnitudesB = thinnedEdges(b);
        const auto lenientDensity =
            std::min(1.0, options.density * options.lenience);
        const auto strictA = keepStrongest(magnitudesA, options.density);
        const auto strictB = keepStrongest(magnitudesB, options.density);
        const auto lenientA = keepStrongest(magnitudesA, lenientDensity);
        const auto lenientB = keepStrongest(magnitudesB, lenientDensity);
        const auto nearA = dilate(lenientA, a.width, a.height, options.radius);
        const auto nearB = dilate(lenientB, a.width, a.height, options.radius);

        EdgeComparison result{
            .options = options,
            .width = a.width,
            .height = a.height
        };
        const auto count = strictA.size();
        result.a.assign(count, EdgeState::None);
        result.b.assign(count, EdgeState::None);
        u64 matchedA = 0;
        u64 matchedB = 0;
        for(usize i = 0; i < count; ++i) {
            if(strictA[i] != 0) {
                ++result.edgesA;
                matchedA += nearB[i];
                result.a[i] =
                    nearB[i] != 0 ? EdgeState::Matched : EdgeState::Unmatched;
            }
            if(strictB[i] != 0) {
                ++result.edgesB;
                matchedB += nearA[i];
                result.b[i] =
                    nearA[i] != 0 ? EdgeState::Matched : EdgeState::Unmatched;
            }
        }
        if(result.edgesA > 0) {
            result.aNearB = static_cast<f64>(matchedA) /
                            static_cast<f64>(result.edgesA);
        }
        if(result.edgesB > 0) {
            result.bNearA = static_cast<f64>(matchedB) /
                            static_cast<f64>(result.edgesB);
        }

        rankEdgeTiles(result);
        findEdgeShift(result, lenientA, lenientB);

        return result;
    }

    std::vector<u8> paintEdgeOverlay(
        const EdgeComparison& comparison,
        Rgba8View a
    ) {
        requireComparable(a, a);
        if(a.width != comparison.width || a.height != comparison.height) {
            throw std::invalid_argument(
                std::format(
                    "sizes differ: {}x{} against a {}x{} comparison",
                    a.width,
                    a.height,
                    comparison.width,
                    comparison.height
                )
            );
        }

        const auto width = static_cast<usize>(comparison.width);
        std::vector<u8> pixels(comparison.a.size() * 4);
        const auto paint = [&](usize at, u8 r, u8 g, u8 b) {
            auto* pixel = pixels.data() + at * 4;
            pixel[0] = r;
            pixel[1] = g;
            pixel[2] = b;
            pixel[3] = 255;
        };
        for(usize y = 0; y < comparison.height; ++y) {
            const auto* row = a.pixels + y * a.rowPitch;
            for(usize x = 0; x < width; ++x) {
                const auto at = y * width + x;
                const auto stateA = comparison.a[at];
                const auto stateB = comparison.b[at];
                if(stateA == EdgeState::Unmatched) {
                    paint(at, 255, 0, 255);
                } else if(stateB == EdgeState::Unmatched) {
                    paint(at, 0, 255, 0);
                } else if(stateA == EdgeState::Matched ||
                          stateB == EdgeState::Matched) {
                    paint(at, 160, 160, 160);
                } else {
                    const auto dim =
                        static_cast<u8>(lumaOf(row + x * 4) * 3 / 10);
                    paint(at, dim, dim, dim);
                }
            }
        }

        if(comparison.worstTileEdges > 0) {
            const auto& tile = comparison.worstTile;
            for(auto x = tile.x0; x < tile.x1; ++x) {
                paint(tile.y0 * width + x, 255, 210, 0);
                paint((tile.y1 - 1) * width + x, 255, 210, 0);
            }
            for(auto y = tile.y0; y < tile.y1; ++y) {
                paint(y * width + tile.x0, 255, 210, 0);
                paint(y * width + tile.x1 - 1, 255, 210, 0);
            }
        }

        return pixels;
    }
}
