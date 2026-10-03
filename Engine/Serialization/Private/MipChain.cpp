#include "MipChain.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <stdexcept>
#include <vector>

#include "ColorSpace.hpp"

namespace Crowy
{
    namespace
    {
        struct Level {
            u32 width = 0;
            u32 height = 0;
            usize offset = 0;
        };

        using DecodeTable = std::array<f32, 256>;
        using Levels = std::vector<Level>;

        constexpr u32 TexelBytes = 4;

        // a byte's linear value, once per byte value
        DecodeTable makeDecodeTable() {
            DecodeTable table{};
            for(u32 i = 0; i < table.size(); ++i)
                table[i] = srgbToLinear(static_cast<f32>(i) / 255.0f);

            return table;
        }

        u8 encodeSrgb(f32 linear) {
            const auto encoded = linear <= 0.0031308f
                ? linear * 12.92f
                : 1.055f * std::pow(linear, 1.0f / 2.4f) - 0.055f;

            return static_cast<u8>(std::clamp(encoded * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        u8 encodeUnorm(f32 value) {
            return static_cast<u8>(std::clamp(value * 255.0f + 0.5f, 0.0f, 255.0f));
        }

        // the source span a destination texel covers: [first, last)
        constexpr std::pair<u32, u32> footprint(u32 index, u32 source, u32 destination) {
            const auto first = index * source / destination;
            const auto last = ((index + 1) * source + destination - 1) / destination;

            return {first, std::max(last, first + 1)};
        }

        void downsample(
            const u8* source,
            Level from,
            u8* destination,
            Level to,
            bool srgb,
            const DecodeTable& decode
        ) {
            for(u32 y = 0; y < to.height; ++y) {
                const auto [y0, y1] = footprint(y, from.height, to.height);
                for(u32 x = 0; x < to.width; ++x) {
                    const auto [x0, x1] = footprint(x, from.width, to.width);

                    std::array<f32, TexelBytes> sum{};
                    for(u32 sy = y0; sy < y1; ++sy) {
                        for(u32 sx = x0; sx < x1; ++sx) {
                            const auto* texel = source + (static_cast<usize>(sy) * from.width + sx) * TexelBytes;
                            for(u32 c = 0; c < 3; ++c)
                                sum[c] += srgb ? decode[texel[c]] : static_cast<f32>(texel[c]) / 255.0f;
                            // alpha is linear in both formats
                            sum[3] += static_cast<f32>(texel[3]) / 255.0f;
                        }
                    }

                    const auto count = static_cast<f32>((y1 - y0) * (x1 - x0));
                    auto* out = destination + (static_cast<usize>(y) * to.width + x) * TexelBytes;
                    for(u32 c = 0; c < 3; ++c)
                        out[c] = srgb ? encodeSrgb(sum[c] / count) : encodeUnorm(sum[c] / count);
                    out[3] = encodeUnorm(sum[3] / count);
                }
            }
        }
    }

    void generateMipChain(ImageData& image) {
        const bool srgb = image.format == RHIPixelFormat::RGBA8_UNORM_SRGB;
        if(!srgb && image.format != RHIPixelFormat::RGBA8_UNORM)
            throw std::invalid_argument("generateMipChain: only RGBA8 images");
        if(image.mipLevels != 1 || image.arraySize != 1 || image.width == 0 || image.height == 0)
            throw std::invalid_argument("generateMipChain: only a single non-empty level");

        const auto levelCount = static_cast<u32>(std::bit_width(std::max(image.width, image.height)));
        Levels levels;
        usize bytes = 0;
        for(u32 i = 0; i < levelCount; ++i) {
            const Level level{
                .width = std::max(1u, image.width >> i),
                .height = std::max(1u, image.height >> i),
                .offset = bytes
            };
            bytes += static_cast<usize>(level.width) * level.height * TexelBytes;
            levels.push_back(level);
        }

        std::vector<u8> blob(bytes);
        std::copy_n(image.blob.begin(), static_cast<usize>(image.width) * image.height * TexelBytes, blob.begin());
        const auto decode = makeDecodeTable();
        for(u32 i = 1; i < levelCount; ++i) {
            downsample(
                blob.data() + levels[i - 1].offset,
                levels[i - 1],
                blob.data() + levels[i].offset,
                levels[i],
                srgb,
                decode
            );
        }

        image.blob = std::move(blob);
        image.mipLevels = levelCount;
        image.subs.clear();
        for(const auto& level: levels) {
            image.subs.push_back(RHISubresourceData{
                .data = image.blob.data() + level.offset,
                .rowPitch = static_cast<usize>(level.width) * TexelBytes
            });
        }
    }
}
