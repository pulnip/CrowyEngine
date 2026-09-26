#include "ImageView.hpp"

#include <format>
#include <stdexcept>

namespace Crowy
{
    Rgba8View viewRgba8(const ImageData& image) {
        using enum RHIPixelFormat;

        if(image.format != RGBA8_UNORM && image.format != RGBA8_UNORM_SRGB) {
            throw std::invalid_argument(
                std::format(
                    "not an RGBA8 image (pixel format {})",
                    static_cast<u32>(image.format)
                )
            );
        }
        if(image.mipLevels != 1 || image.arraySize != 1) {
            throw std::invalid_argument(
                std::format(
                    "{} mip levels and {} slices are not one picture",
                    image.mipLevels,
                    image.arraySize
                )
            );
        }
        if(image.width == 0 || image.height == 0) {
            throw std::invalid_argument(
                std::format("an empty image ({}x{})", image.width, image.height)
            );
        }

        const auto rowPitch = static_cast<usize>(image.width) * 4;
        if(image.blob.size() < rowPitch * image.height) {
            throw std::invalid_argument(
                std::format(
                    "{} bytes cannot hold {}x{} RGBA8 pixels",
                    image.blob.size(),
                    image.width,
                    image.height
                )
            );
        }

        return Rgba8View{
            .pixels = image.blob.data(),
            .width = image.width,
            .height = image.height,
            .rowPitch = rowPitch
        };
    }
}
