#include <array>
#include <cmath>
#include <functional>
#include <limits>
#include <set>
#include <stdexcept>
#include <utility>

#include <gtest/gtest.h>

#include "ImageCompare.hpp"
#include "ImageLoader.hpp"
#include "ImageView.hpp"

using namespace Crowy;

namespace
{
    // one colour, tightly packed, subs[0] into blob; move it, never copy it
    ImageData makeImage(u32 width, u32 height, u8 r, u8 g, u8 b, u8 a = 255) {
        ImageData image{
            .format = RHIPixelFormat::RGBA8_UNORM_SRGB,
            .width = width,
            .height = height
        };
        image.blob.resize(static_cast<usize>(width) * height * 4);

        for(usize i = 0; i < image.blob.size(); i += 4) {
            image.blob[i + 0] = r;
            image.blob[i + 1] = g;
            image.blob[i + 2] = b;
            image.blob[i + 3] = a;
        }
        image.subs.push_back(
            RHISubresourceData{
                .data = image.blob.data(),
                .rowPitch = static_cast<usize>(width) * 4
            }
        );

        return image;
    }

    u8& channelAt(ImageData& image, u32 x, u32 y, u32 channel) {
        const auto pixel = static_cast<usize>(y) * image.width + x;

        return image.blob[pixel * 4 + channel];
    }

    // a gray level per pixel from `level(x, y)`
    ImageData grayImage(
        u32 width,
        u32 height,
        const std::function<u8(u32, u32)>& level
    ) {
        auto image = makeImage(width, height, 0, 0, 0);
        for(u32 y = 0; y < height; ++y) {
            for(u32 x = 0; x < width; ++x) {
                const auto v = level(x, y);
                for(u32 c = 0; c < 3; ++c)
                    channelAt(image, x, y, c) = v;
            }
        }

        return image;
    }

    // `level` inside [x0, x1) x [y0, y1), 0 outside
    std::function<u8(u32, u32)> box(u32 x0, u32 y0, u32 x1, u32 y1) {
        return [=](u32 x, u32 y) -> u8 {
            return x >= x0 && x < x1 && y >= y0 && y < y1 ? 200 : 0;
        };
    }

    usize countOf(const std::vector<u8>& mask) {
        usize count = 0;
        for(const auto m: mask)
            count += m;

        return count;
    }

    std::set<u32> columnsOf(const std::vector<u8>& mask, u32 width) {
        std::set<u32> columns;
        for(usize i = 0; i < mask.size(); ++i) {
            if(mask[i] != 0)
                columns.insert(static_cast<u32>(i % width));
        }

        return columns;
    }

    void expectExact(const ChannelStats& stats) {
        EXPECT_EQ(stats.max, 0);
        EXPECT_EQ(stats.mean, 0.0);
        EXPECT_EQ(stats.rms, 0.0);
        EXPECT_TRUE(std::isinf(stats.psnr));
    }
}

TEST(ImageCompare, IdenticalIsSimilar) {
    const auto a = makeImage(16, 16, 10, 20, 30);
    const auto b = makeImage(16, 16, 10, 20, 30);

    const auto result = compareImages(a, b);

    EXPECT_TRUE(result.similar);
    EXPECT_EQ(result.failing, 0u);
    EXPECT_EQ(result.globalFail, 0.0);
    EXPECT_EQ(result.localFail, 0.0);
    ASSERT_EQ(result.delta.size(), 256u);
    for(const auto d: result.delta)
        EXPECT_EQ(d, 0);
    for(usize c = 0; c < 3; ++c)
        expectExact(result.channel[c]);
}

TEST(ImageCompare, OnePixelOverTolerance) {
    const auto a = makeImage(4, 4, 100, 100, 100);
    auto b = makeImage(4, 4, 100, 100, 100);
    channelAt(b, 1, 2, 0) = 103;

    const auto strict = compareImages(a, b, {.channelDelta = 2});

    EXPECT_FALSE(strict.similar);
    EXPECT_EQ(strict.failing, 1u);
    EXPECT_EQ(strict.globalFail, 0.0625);
    // a four-pixel side leaves four one-pixel tiles of the ten
    EXPECT_EQ(strict.localFail, 1.0);
    EXPECT_EQ(strict.tileX, 4u);
    EXPECT_EQ(strict.tileY, 7u);
    EXPECT_EQ(strict.worstTile.x0, 1u);
    EXPECT_EQ(strict.worstTile.y0, 2u);
    EXPECT_EQ(strict.worstTile.x1, 2u);
    EXPECT_EQ(strict.worstTile.y1, 3u);
    ASSERT_EQ(strict.delta.size(), 16u);
    for(usize i = 0; i < strict.delta.size(); ++i)
        EXPECT_EQ(strict.delta[i], i == 9 ? 3 : 0) << "pixel " << i;
    EXPECT_EQ(strict.channel[0].max, 3);
    EXPECT_EQ(strict.channel[0].mean, 0.1875);
    EXPECT_EQ(strict.channel[0].rms, 0.75);
    EXPECT_NEAR(strict.channel[0].psnr, 50.630, 1e-3);
    expectExact(strict.channel[1]);
    expectExact(strict.channel[2]);

    const auto loose = compareImages(a, b, {.channelDelta = 3});

    EXPECT_TRUE(loose.similar);
    EXPECT_EQ(loose.failing, 0u);
    // the delta plane does not depend on the tolerance
    EXPECT_EQ(loose.delta[9], 3);
}

TEST(ImageCompare, AlphaIgnoredUnlessRequested) {
    const auto a = makeImage(8, 8, 50, 60, 70, 255);
    const auto b = makeImage(8, 8, 50, 60, 70, 100);

    const auto ignored = compareImages(a, b);

    EXPECT_TRUE(ignored.similar);
    EXPECT_EQ(ignored.failing, 0u);
    EXPECT_EQ(ignored.channel[3].max, 0);

    const auto compared = compareImages(a, b, {.compareAlpha = true});

    EXPECT_FALSE(compared.similar);
    EXPECT_EQ(compared.failing, 64u);
    EXPECT_EQ(compared.globalFail, 1.0);
    EXPECT_EQ(compared.channel[3].max, 155);
    EXPECT_EQ(compared.channel[3].mean, 155.0);
    EXPECT_EQ(compared.channel[3].rms, 155.0);
    EXPECT_NEAR(compared.channel[3].psnr, 4.324, 1e-3);
}

TEST(ImageCompare, SizeMismatchThrows) {
    const auto a = makeImage(4, 4, 0, 0, 0);
    const auto b = makeImage(4, 5, 0, 0, 0);
    const auto empty = makeImage(0, 4, 0, 0, 0);

    EXPECT_THROW(compareImages(a, b), std::invalid_argument);
    EXPECT_THROW(compareImages(empty, empty), std::invalid_argument);
}

TEST(ImageCompare, FormatMismatchThrows) {
    auto bgra = makeImage(2, 2, 0, 0, 0);
    bgra.format = RHIPixelFormat::BGRA8_UNORM;
    auto mipped = makeImage(2, 2, 0, 0, 0);
    mipped.mipLevels = 2;

    EXPECT_THROW(viewRgba8(bgra), std::invalid_argument);
    EXPECT_THROW(viewRgba8(mipped), std::invalid_argument);

    // the colour-space tag says how to sample, not what the bytes are
    auto linear = makeImage(2, 2, 7, 8, 9);
    linear.format = RHIPixelFormat::RGBA8_UNORM;
    const auto srgb = makeImage(2, 2, 7, 8, 9);

    EXPECT_TRUE(compareImages(linear, srgb).similar);
}

TEST(ImageCompare, GlobalThresholdInclusive) {
    constexpr ImageTolerance GlobalOnly{
        .channelDelta = 2,
        .maxGlobalFail = 0.001,
        .maxLocalFail = 1.0
    };
    const auto a = makeImage(100, 100, 50, 50, 50);
    auto b = makeImage(100, 100, 50, 50, 50);
    for(u32 k = 0; k < 10; ++k)
        channelAt(b, 10 * k, 0, 0) = 60;

    const auto atLimit = compareImages(a, b, GlobalOnly);

    EXPECT_EQ(atLimit.failing, 10u);
    EXPECT_EQ(atLimit.globalFail, 0.001);
    EXPECT_TRUE(atLimit.similar);

    channelAt(b, 5, 50, 0) = 60;
    const auto over = compareImages(a, b, GlobalOnly);

    EXPECT_EQ(over.failing, 11u);
    EXPECT_DOUBLE_EQ(over.globalFail, 0.0011);
    EXPECT_FALSE(over.similar);
}

TEST(ImageCompare, LocalThresholdInclusive) {
    constexpr ImageTolerance LocalOnly{
        .channelDelta = 2,
        .maxGlobalFail = 1.0,
        .maxLocalFail = 0.01
    };
    const auto a = makeImage(200, 200, 50, 50, 50);
    auto b = makeImage(200, 200, 50, 50, 50);
    // tile (3,7) of the 10 x 10 grid covers x 60..80, y 140..160
    constexpr std::array<std::pair<u32, u32>, 4> InTile = {
        {{60, 140}, {61, 140}, {60, 141}, {75, 155}}
    };
    for(const auto [x, y]: InTile)
        channelAt(b, x, y, 0) = 60;

    const auto atLimit = compareImages(a, b, LocalOnly);

    EXPECT_EQ(atLimit.localFail, 0.01);
    EXPECT_TRUE(atLimit.similar);
    EXPECT_EQ(atLimit.tileX, 3u);
    EXPECT_EQ(atLimit.tileY, 7u);
    EXPECT_EQ(atLimit.worstTile.x0, 60u);
    EXPECT_EQ(atLimit.worstTile.y0, 140u);
    EXPECT_EQ(atLimit.worstTile.x1, 80u);
    EXPECT_EQ(atLimit.worstTile.y1, 160u);

    channelAt(b, 70, 150, 0) = 60;
    const auto over = compareImages(a, b, LocalOnly);

    EXPECT_DOUBLE_EQ(over.localFail, 0.0125);
    EXPECT_FALSE(over.similar);
}

// the concentrated regression a global fraction averages away
TEST(ImageCompare, ClusterFailsLocalNotGlobal) {
    constexpr ImageTolerance Loose{
        .channelDelta = 2,
        .maxGlobalFail = 0.001,
        .maxLocalFail = 0.01
    };
    const auto a = makeImage(200, 200, 50, 50, 50);
    auto b = makeImage(200, 200, 50, 50, 50);
    for(u32 k = 0; k < 11; ++k)
        channelAt(b, 60 + k, 140, 0) = 60;

    const auto result = compareImages(a, b, Loose);

    EXPECT_EQ(result.failing, 11u);
    EXPECT_DOUBLE_EQ(result.globalFail, 0.000275);
    EXPECT_LE(result.globalFail, Loose.maxGlobalFail);
    EXPECT_DOUBLE_EQ(result.localFail, 0.0275);
    EXPECT_EQ(result.tileX, 3u);
    EXPECT_EQ(result.tileY, 7u);
    EXPECT_FALSE(result.similar);
}

// one backend repeats a frame byte for byte, so the defaults let no pixel
// fail, however large the image
TEST(ImageCompare, DefaultsLetNoPixelFail) {
    const auto a = makeImage(200, 200, 50, 50, 50);
    auto b = makeImage(200, 200, 50, 50, 50);
    channelAt(b, 120, 30, 1) = 52;

    EXPECT_TRUE(compareImages(a, b).similar);

    channelAt(b, 120, 30, 1) = 53;
    const auto result = compareImages(a, b);

    EXPECT_EQ(result.failing, 1u);
    EXPECT_DOUBLE_EQ(result.globalFail, 0.000025);
    EXPECT_FALSE(result.similar);
}

TEST(ImageCompare, ChannelStatsKnownOffset) {
    const auto a = makeImage(16, 16, 100, 100, 100);
    const auto b = makeImage(16, 16, 104, 100, 100);

    const auto result = compareImages(a, b);

    EXPECT_EQ(result.channel[0].max, 4);
    EXPECT_EQ(result.channel[0].mean, 4.0);
    EXPECT_EQ(result.channel[0].rms, 4.0);
    EXPECT_NEAR(result.channel[0].psnr, 36.090, 1e-3);
    expectExact(result.channel[1]);
    expectExact(result.channel[2]);
    EXPECT_EQ(result.failing, 256u);
    EXPECT_EQ(result.globalFail, 1.0);
    EXPECT_EQ(result.localFail, 1.0);
}

TEST(ImageCompare, HeatMapColours) {
    const auto a = makeImage(4, 1, 100, 100, 100);
    auto b = makeImage(4, 1, 100, 100, 100);
    channelAt(b, 1, 0, 0) = 101;
    channelAt(b, 2, 0, 0) = 102;
    channelAt(b, 3, 0, 0) = 109;

    const auto heat = paintHeatMap(compareImages(a, b, {.channelDelta = 2}));

    ASSERT_EQ(heat.size(), 16u);
    for(usize i = 0; i < 4; ++i)
        EXPECT_EQ(heat[i * 4 + 3], 255) << "pixel " << i;

    // equal: black
    EXPECT_EQ(heat[0], 0);
    EXPECT_EQ(heat[1], 0);
    EXPECT_EQ(heat[2], 0);

    // within the tolerance: a grey apart from black and white
    EXPECT_EQ(heat[4], heat[5]);
    EXPECT_EQ(heat[5], heat[6]);
    EXPECT_GT(heat[4], 0);
    EXPECT_LT(heat[4], 255);

    // at the tolerance: white
    EXPECT_EQ(heat[8], 255);
    EXPECT_EQ(heat[9], 255);
    EXPECT_EQ(heat[10], 255);

    // failing: full red, no blue
    EXPECT_EQ(heat[12], 255);
    EXPECT_EQ(heat[14], 0);
}

// a step thins to one column, on its later side
TEST(ImageCompare, EdgesOfAStepAreOneColumn) {
    const auto image =
        grayImage(32, 16, [](u32 x, u32) -> u8 { return x >= 16 ? 200 : 0; });

    const auto edges = findEdges(viewRgba8(image), 1.0);

    EXPECT_EQ(countOf(edges), 16u);
    EXPECT_EQ(columnsOf(edges, 32), std::set<u32>{16});
}

// the budget keeps the strongest edges by count
TEST(ImageCompare, EdgesKeepTheStrongestByCount) {
    const auto image = grayImage(64, 64, [](u32 x, u32) -> u8 {
        return x < 20 ? 0 : (x < 40 ? 200 : 160);
    });

    const auto strongest = findEdges(viewRgba8(image), 1.0 / 64.0);
    const auto both = findEdges(viewRgba8(image), 2.0 / 64.0);

    EXPECT_EQ(countOf(strongest), 64u);
    EXPECT_EQ(columnsOf(strongest, 64), std::set<u32>{20});
    EXPECT_EQ(countOf(both), 128u);
    EXPECT_EQ(columnsOf(both, 64), (std::set<u32>{20, 40}));
}

// a tone curve moves no step
TEST(ImageCompare, EdgesIgnoreAToneCurve) {
    const auto nested = [](u32 x, u32 y) -> u8 {
        if(x >= 28 && x < 36 && y >= 28 && y < 36)
            return 220;
        if(x >= 16 && x < 48 && y >= 16 && y < 48)
            return 120;
        return 30;
    };
    const auto curved = [&](u32 x, u32 y) {
        const auto v = static_cast<f64>(nested(x, y)) / 255.0;
        return static_cast<u8>(std::lround(255.0 * std::pow(v, 1.0 / 2.2)));
    };

    const auto edges = findEdges(viewRgba8(grayImage(64, 64, nested)), 1.0);
    const auto curvedEdges =
        findEdges(viewRgba8(grayImage(64, 64, curved)), 1.0);

    EXPECT_EQ(countOf(edges), 152u);
    EXPECT_EQ(edges, curvedEdges);
}

// a symmetric blur leaves the step's peak where it was
TEST(ImageCompare, EdgesHoldUnderASymmetricBlur) {
    // a 0 to 200 step at 16 blurred by [1 4 6 4 1] / 16
    const auto image = grayImage(32, 16, [](u32 x, u32) -> u8 {
        constexpr std::array<u8, 4> Ramp = {13, 63, 138, 188};
        if(x <= 13)
            return 0;
        if(x >= 18)
            return 200;
        return Ramp[x - 14];
    });

    const auto edges = findEdges(viewRgba8(image), 1.0);

    EXPECT_EQ(countOf(edges), 16u);
    EXPECT_EQ(columnsOf(edges, 32), std::set<u32>{16});
}

// within the radius an edge finds the other's; past it, few do
TEST(ImageCompare, EdgesAgreeWithinTheRadius) {
    const auto bar = grayImage(100, 100, box(40, 30, 50, 70));
    const auto near = grayImage(100, 100, box(42, 30, 52, 70));
    const auto far = grayImage(100, 100, box(44, 30, 54, 70));

    const auto close = compareEdges(viewRgba8(bar), viewRgba8(near));
    const auto apart = compareEdges(viewRgba8(bar), viewRgba8(far));

    EXPECT_EQ(close.edgesA, 96u);
    EXPECT_EQ(close.edgesB, 96u);
    EXPECT_EQ(close.aNearB, 1.0);
    EXPECT_EQ(close.bNearA, 1.0);
    EXPECT_DOUBLE_EQ(apart.aNearB, 19.0 / 96.0);
    EXPECT_DOUBLE_EQ(apart.bNearA, 21.0 / 96.0);
}

// the searched side's lenience forgives edges a tone curve reorders
TEST(ImageCompare, EdgeLenienceForgivesReorderedStrengths) {
    const auto a = grayImage(64, 64, [](u32 x, u32) -> u8 {
        return x < 20 ? 0 : (x < 40 ? 200 : 160);
    });
    const auto b = grayImage(64, 64, [](u32 x, u32) -> u8 {
        return x < 20 ? 0 : (x < 40 ? 40 : 240);
    });

    const auto strict = compareEdges(
        viewRgba8(a),
        viewRgba8(b),
        {.density = 1.0 / 64.0, .lenience = 1.0}
    );
    const auto lenient = compareEdges(
        viewRgba8(a),
        viewRgba8(b),
        {.density = 1.0 / 64.0, .lenience = 2.0}
    );

    EXPECT_EQ(strict.aNearB, 0.0);
    EXPECT_EQ(strict.bNearA, 0.0);
    EXPECT_EQ(lenient.aNearB, 1.0);
    EXPECT_EQ(lenient.bNearA, 1.0);
}

// the shift search finds how far B's content sits from A's
TEST(ImageCompare, EdgeShiftFindsAMovedPicture) {
    const auto bar = grayImage(100, 100, box(40, 30, 50, 70));
    const auto moved = grayImage(100, 100, box(43, 28, 53, 68));

    const auto shifted = compareEdges(viewRgba8(bar), viewRgba8(moved));
    const auto same = compareEdges(viewRgba8(bar), viewRgba8(bar));

    EXPECT_EQ(shifted.shiftX, 3);
    EXPECT_EQ(shifted.shiftY, -2);
    EXPECT_EQ(shifted.shiftCoincide, 1.0);
    EXPECT_DOUBLE_EQ(shifted.zeroCoincide, 4.0 / 192.0);
    EXPECT_EQ(same.shiftX, 0);
    EXPECT_EQ(same.shiftY, 0);
    EXPECT_EQ(same.shiftCoincide, 1.0);
    EXPECT_EQ(same.zeroCoincide, 1.0);
}

// the worst tile holds what moved
TEST(ImageCompare, EdgeWorstTileHoldsTheMovedBar) {
    const auto scene = [](u32 dx) {
        return grayImage(200, 200, [dx](u32 x, u32 y) -> u8 {
            const bool square = x >= 45 && x < 55 && y >= 45 && y < 55;
            const bool bar =
                x >= 145 + dx && x < 151 + dx && y >= 122 && y < 138;
            return square || bar ? 200 : 0;
        });
    };
    const auto a = scene(0);
    const auto b = scene(4);

    const auto result =
        compareEdges(viewRgba8(a), viewRgba8(b), {.minTileEdges = 10});

    EXPECT_EQ(result.edgesA, 76u);
    EXPECT_DOUBLE_EQ(result.aNearB, 58.0 / 76.0);
    EXPECT_EQ(result.tileX, 7u);
    EXPECT_EQ(result.tileY, 6u);
    EXPECT_EQ(result.worstTile.x0, 140u);
    EXPECT_EQ(result.worstTile.y0, 120u);
    EXPECT_EQ(result.worstTile.x1, 160u);
    EXPECT_EQ(result.worstTile.y1, 140u);
    EXPECT_EQ(result.worstTileEdges, 40u);
    EXPECT_DOUBLE_EQ(result.worstTileNear, 22.0 / 40.0);
}

TEST(ImageCompare, EdgeOverlayColors) {
    const auto stepAt = [](u32 at) {
        return grayImage(32, 32, [at](u32 x, u32) -> u8 {
            return x >= at ? 200 : 0;
        });
    };
    const auto a = stepAt(16);
    const auto b = stepAt(20);
    const EdgeOptions untiled{
        .minTileEdges = std::numeric_limits<u32>::max()
    };
    const auto pixelAt = [](const std::vector<u8>& overlay, u32 x, u32 y) {
        const auto* p = overlay.data() + (static_cast<usize>(y) * 32 + x) * 4;
        return std::array<u8, 4>{p[0], p[1], p[2], p[3]};
    };

    const auto apart = paintEdgeOverlay(
        compareEdges(viewRgba8(a), viewRgba8(b), untiled),
        viewRgba8(a)
    );
    const auto same = paintEdgeOverlay(
        compareEdges(viewRgba8(a), viewRgba8(a), untiled),
        viewRgba8(a)
    );

    using Pixel = std::array<u8, 4>;
    EXPECT_EQ(pixelAt(apart, 16, 5), (Pixel{255, 0, 255, 255}));
    EXPECT_EQ(pixelAt(apart, 20, 5), (Pixel{0, 255, 0, 255}));
    EXPECT_EQ(pixelAt(apart, 10, 5), (Pixel{0, 0, 0, 255}));
    EXPECT_EQ(pixelAt(apart, 25, 5), (Pixel{60, 60, 60, 255}));
    EXPECT_EQ(pixelAt(same, 16, 5), (Pixel{160, 160, 160, 255}));
}

TEST(ImageCompare, EdgeSizeMismatchThrows) {
    const auto a = makeImage(4, 4, 0, 0, 0);
    const auto b = makeImage(4, 5, 0, 0, 0);

    EXPECT_THROW(
        compareEdges(viewRgba8(a), viewRgba8(b)),
        std::invalid_argument
    );
}
