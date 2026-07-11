#include <cstring>
#include <utility>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Assert.hpp"
#include "ImageLoader.hpp"
#include "LogLocal.hpp"
#include "StringUtil.hpp"

namespace{
    auto convert(Crowy::ImageFormat format){
        using enum Crowy::ImageFormat;

        switch(format){
        case RGBA:
            return STBI_rgb_alpha;
        default:
            std::unreachable();
        }
    }

}

namespace Crowy
{
    u32 toChannels(ImageFormat format) noexcept{
        using enum ImageFormat;

        switch(format){
        case RGBA:
            return 4;
        default:
            std::unreachable();
        }
    }

    ImageData loadImage(const std::filesystem::path& path, ImageFormat desiredFormat){
        auto bin = readFileAsBinary(path);

        int width = 0, height = 0, _ = 0;
        auto data = stbi_load_from_memory(
            bin.data(), static_cast<int>(bin.size()),
            &width, &height, &_,
            ::convert(desiredFormat)
        );

        if(data == nullptr){
            LOG_ERROR("Image load Failed: {} - {}",
                path, stbi_failure_reason()
            );
        }

        auto size = width * height * toChannels(desiredFormat);
        ImageBlob blob(size);
        std::memcpy(blob.data(), data, size);

        stbi_image_free(data);

        return ImageData(std::move(blob),
            static_cast<u32>(width),
            static_cast<u32>(height),
            desiredFormat
        );
    }
}
