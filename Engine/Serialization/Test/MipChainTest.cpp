#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "MipChain.hpp"

using namespace Crowy;

namespace
{
    // a size x size image, texel (x, y) from `texel`
    template<typename F>
    ImageData makeImage(u32 width, u32 height, RHIPixelFormat format, F&& texel) {
        ImageData image{.format = format, .width = width, .height = height};
        image.blob.resize(static_cast<usize>(width) * height * 4);
        for(u32 y = 0; y < height; ++y) {
            for(u32 x = 0; x < width; ++x) {
                const auto value = texel(x, y);
                auto* out = image.blob.data() + (static_cast<usize>(y) * width + x) * 4;
                out[0] = value;
                out[1] = value;
                out[2] = value;
                out[3] = 255;
            }
        }
        image.subs.push_back(RHISubresourceData{.data = image.blob.data(), .rowPitch = width * 4});

        return image;
    }

    u8 checker(u32 x, u32 y) {
        return (x + y) % 2 == 0 ? 0 : 255;
    }

    const u8* level(const ImageData& image, u32 index) {
        return static_cast<const u8*>(image.subs[index].data);
    }
}

// half black, half white averages to linear 0.5, which sRGB encodes as 188
TEST(MipChain, SrgbAveragesInLinearLight) {
    auto image = makeImage(4, 4, RHIPixelFormat::RGBA8_UNORM_SRGB, checker);
    generateMipChain(image);

    ASSERT_EQ(image.mipLevels, 3u);
    const auto* mip1 = level(image, 1);
    for(u32 i = 0; i < 2 * 2; ++i) {
        EXPECT_NEAR(mip1[i * 4], 188, 1) << i;
        EXPECT_EQ(mip1[i * 4 + 3], 255);
    }
    EXPECT_NEAR(level(image, 2)[0], 188, 1);
}

TEST(MipChain, UnormAveragesTheBytes) {
    auto image = makeImage(4, 4, RHIPixelFormat::RGBA8_UNORM, checker);
    generateMipChain(image);

    EXPECT_NEAR(level(image, 1)[0], 128, 1);
}

// every level down to 1 x 1, each sub inside the blob with a tight pitch
TEST(MipChain, LevelsRunDownToOne) {
    struct Case {
        u32 width;
        u32 height;
        u32 levels;
    };
    for(const auto [width, height, levels]: {Case{2048, 1024, 12}, Case{1536, 1728, 11}, Case{8, 8, 4}, Case{5, 3, 3}}) {
        auto image = makeImage(width, height, RHIPixelFormat::RGBA8_UNORM_SRGB, checker);
        generateMipChain(image);

        ASSERT_EQ(image.mipLevels, levels) << width << "x" << height;
        ASSERT_EQ(image.subs.size(), levels);
        const auto* begin = image.blob.data();
        const auto* end = begin + image.blob.size();
        for(u32 i = 0; i < levels; ++i) {
            const auto levelWidth = std::max(1u, width >> i);
            const auto levelHeight = std::max(1u, height >> i);
            const auto* data = level(image, i);
            EXPECT_EQ(image.subs[i].rowPitch, levelWidth * 4u);
            EXPECT_GE(data, begin);
            EXPECT_LE(data + static_cast<usize>(levelWidth) * levelHeight * 4, end);
        }
        EXPECT_EQ(std::max(1u, width >> (levels - 1)), 1u);
        EXPECT_EQ(std::max(1u, height >> (levels - 1)), 1u);
    }
}

TEST(MipChain, TheFirstLevelIsUntouched) {
    auto image = makeImage(4, 4, RHIPixelFormat::RGBA8_UNORM_SRGB, [](u32 x, u32 y) {
        return static_cast<u8>(x * 16 + y);
    });
    const auto before = image.blob;
    generateMipChain(image);

    EXPECT_TRUE(std::equal(before.begin(), before.end(), image.blob.begin()));
}

TEST(MipChain, RefusesWhatItCannotFilter) {
    auto other = makeImage(4, 4, RHIPixelFormat::RGBA16_FLOAT, checker);
    EXPECT_THROW(generateMipChain(other), std::invalid_argument);

    auto mipped = makeImage(4, 4, RHIPixelFormat::RGBA8_UNORM, checker);
    generateMipChain(mipped);
    EXPECT_THROW(generateMipChain(mipped), std::invalid_argument);
}
