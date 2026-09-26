#include "ImageWriter.hpp"

#include <format>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

// the file variants open through narrow C stdio; this writes through a stream
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "StringUtil.hpp"

namespace
{
    void appendToStream(void* context, void* data, int size) {
        static_cast<std::ofstream*>(context)->write(
            static_cast<const char*>(data),
            size
        );
    }
}

namespace Crowy
{
    void writePng(
        const std::filesystem::path& path,
        Rgba8View image,
        bool alpha
    ) {
        if(image.pixels == nullptr || image.width == 0 || image.height == 0) {
            throw std::invalid_argument(
                std::format("an empty image ({}x{})", image.width, image.height)
            );
        }

        const usize channels = alpha ? 4 : 3;
        // stb takes int sizes
        constexpr auto IntMax =
            static_cast<usize>(std::numeric_limits<int>::max());

        if(image.rowPitch > IntMax || image.height > IntMax) {
            throw std::invalid_argument(
                std::format(
                    "{}x{} is too large to write as PNG",
                    image.width,
                    image.height
                )
            );
        }

        const u8* rows = image.pixels;
        auto rowPitch = image.rowPitch;
        std::vector<u8> packed;

        if(!alpha) {
            rowPitch = static_cast<usize>(image.width) * channels;
            packed.resize(rowPitch * image.height);
            for(u32 y = 0; y < image.height; ++y) {
                const auto* source = image.pixels + y * image.rowPitch;
                auto* target = packed.data() + y * rowPitch;
                for(u32 x = 0; x < image.width; ++x) {
                    target[x * 3 + 0] = source[x * 4 + 0];
                    target[x * 3 + 1] = source[x * 4 + 1];
                    target[x * 3 + 2] = source[x * 4 + 2];
                }
            }
            rows = packed.data();
        }

        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file) {
            throw std::runtime_error(
                std::format("cannot open '{}' for writing", toUTF8String(path))
            );
        }

        const auto encoded = stbi_write_png_to_func(
            appendToStream,
            &file,
            static_cast<int>(image.width),
            static_cast<int>(image.height),
            static_cast<int>(channels),
            rows,
            static_cast<int>(rowPitch)
        );

        file.close();
        if(encoded == 0 || !file) {
            throw std::runtime_error(
                std::format("cannot write '{}'", toUTF8String(path))
            );
        }
    }
}
