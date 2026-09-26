#include <array>
#include <cmath>
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
    const auto a = makeImage(200, 200, 50, 50, 50);
    auto b = makeImage(200, 200, 50, 50, 50);
    for(u32 k = 0; k < 11; ++k)
        channelAt(b, 60 + k, 140, 0) = 60;

    const auto result = compareImages(a, b);

    EXPECT_EQ(result.failing, 11u);
    EXPECT_DOUBLE_EQ(result.globalFail, 0.000275);
    EXPECT_DOUBLE_EQ(result.localFail, 0.0275);
    EXPECT_EQ(result.tileX, 3u);
    EXPECT_EQ(result.tileY, 7u);
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
