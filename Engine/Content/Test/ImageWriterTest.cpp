#include <filesystem>
#include <vector>

#include <gtest/gtest.h>

#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "ImageWriter.hpp"

using namespace Crowy;

// Rows carry padding past their pixels, so both the packing path (RGB) and
// the stride path (RGBA) have to honour rowPitch.
TEST(ImageWriter, PngRoundTrip) {
    constexpr u32 Width = 8;
    constexpr u32 Height = 8;
    constexpr usize RowPitch = Width * 4 + 12;

    std::vector<u8> pixels(RowPitch * Height, 0xCD);
    for(u32 y = 0; y < Height; ++y) {
        for(u32 x = 0; x < Width; ++x) {
            auto* pixel = pixels.data() + y * RowPitch + x * 4;
            pixel[0] = static_cast<u8>(x * 32);
            pixel[1] = static_cast<u8>(y * 32);
            pixel[2] = static_cast<u8>((x + y) * 16);
            pixel[3] = static_cast<u8>(255 - x * 8 - y);
        }
    }
    const Rgba8View view{
        .pixels = pixels.data(),
        .width = Width,
        .height = Height,
        .rowPitch = RowPitch
    };

    const auto directory = std::filesystem::temp_directory_path();
    const auto rgbPath = directory / "ImageWriterTest_rgb.png";
    const auto rgbaPath = directory / "ImageWriterTest_rgba.png";
    writePng(rgbPath, view);
    writePng(rgbaPath, view, true);
    const auto rgb = LoadImage(rgbPath);
    const auto rgba = LoadImage(rgbaPath);
    std::filesystem::remove(rgbPath);
    std::filesystem::remove(rgbaPath);

    ASSERT_EQ(rgb.width, Width);
    ASSERT_EQ(rgb.height, Height);
    ASSERT_EQ(rgba.width, Width);
    ASSERT_EQ(rgba.height, Height);
    for(u32 y = 0; y < Height; ++y) {
        for(u32 x = 0; x < Width; ++x) {
            const auto* source = pixels.data() + y * RowPitch + x * 4;
            const auto at = (static_cast<usize>(y) * Width + x) * 4;
            for(usize c = 0; c < 3; ++c) {
                EXPECT_EQ(rgb.blob[at + c], source[c]) << x << "," << y;
                EXPECT_EQ(rgba.blob[at + c], source[c]) << x << "," << y;
            }
            EXPECT_EQ(rgb.blob[at + 3], 255) << x << "," << y;
            EXPECT_EQ(rgba.blob[at + 3], source[3]) << x << "," << y;
        }
    }
}
