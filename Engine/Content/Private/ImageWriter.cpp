#include "ImageWriter.hpp"

#include <array>
#include <format>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <vector>

// the file variants open through narrow C stdio; this writes through a stream
#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "PathUtil.hpp"

namespace
{
    using namespace Crowy;

    using Swizzle = std::array<usize, 4>;

    constexpr Swizzle SameOrder = {0, 1, 2, 3};
    constexpr Swizzle SwappedRedBlue = {2, 1, 0, 3};

    void appendToStream(void* context, void* data, int size) {
        static_cast<std::ofstream*>(context)->write(
            static_cast<const char*>(data),
            size
        );
    }

    void requirePixels(Rgba8View image) {
        if(image.pixels == nullptr || image.width == 0 || image.height == 0) {
            throw std::invalid_argument(
                std::format("an empty image ({}x{})", image.width, image.height)
            );
        }
    }

    std::vector<u8> pack(Rgba8View image, usize channels, const Swizzle& from) {
        const auto rowSize = usize{image.width} * channels;
        std::vector<u8> packed(rowSize * image.height);
        for(u32 y = 0; y < image.height; ++y) {
            const auto* source = image.pixels + y * image.rowPitch;
            auto* target = packed.data() + y * rowSize;
            for(u32 x = 0; x < image.width; ++x) {
                for(usize c = 0; c < channels; ++c)
                    target[x * channels + c] = source[x * 4 + from[c]];
            }
        }

        return packed;
    }

    std::ofstream openForWriting(const std::filesystem::path& path) {
        std::ofstream file(path, std::ios::binary | std::ios::trunc);
        if(!file) {
            throw std::runtime_error(
                std::format("cannot open '{}' for writing", toUTF8String(path))
            );
        }

        return file;
    }

    void finishWriting(
        std::ofstream& file,
        const std::filesystem::path& path,
        bool encoded
    ) {
        file.close();
        if(!encoded || !file) {
            throw std::runtime_error(
                std::format("cannot write '{}'", toUTF8String(path))
            );
        }
    }
}

namespace Crowy
{
    void writePng(
        const std::filesystem::path& path,
        Rgba8View image,
        bool alpha,
        ChannelOrder order
    ) {
        requirePixels(image);

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

        if(!alpha || order == ChannelOrder::Bgra) {
            const auto& from =
                order == ChannelOrder::Rgba ? SameOrder : SwappedRedBlue;
            packed = pack(image, channels, from);
            rows = packed.data();
            rowPitch = usize{image.width} * channels;
        }

        auto file = openForWriting(path);
        const auto encoded = stbi_write_png_to_func(
            appendToStream,
            &file,
            static_cast<int>(image.width),
            static_cast<int>(image.height),
            static_cast<int>(channels),
            rows,
            static_cast<int>(rowPitch)
        );
        finishWriting(file, path, encoded != 0);
    }

    void writeBmp(
        const std::filesystem::path& path,
        Rgba8View image,
        ChannelOrder order
    ) {
        requirePixels(image);

        constexpr u32 HeaderSize = 54;
        const auto imageSize = u64{image.width} * image.height * 4;
        if(HeaderSize + imageSize > std::numeric_limits<u32>::max()) {
            throw std::invalid_argument(
                std::format(
                    "{}x{} is too large to write as BMP",
                    image.width,
                    image.height
                )
            );
        }

        std::array<u8, HeaderSize> header{};
        const auto put16 = [&](usize at, u16 v) {
            header[at] = static_cast<u8>(v & 0xFF);
            header[at + 1] = static_cast<u8>(v >> 8);
        };
        const auto put32 = [&](usize at, u32 v) {
            for(usize i = 0; i < 4; ++i)
                header[at + i] = static_cast<u8>((v >> (8 * i)) & 0xFF);
        };

        header[0] = 'B';
        header[1] = 'M';
        put32(2, HeaderSize + static_cast<u32>(imageSize));
        put32(10, HeaderSize);
        // BITMAPINFOHEADER; a positive height is bottom-up, 0 is BI_RGB
        put32(14, 40);
        put32(18, image.width);
        put32(22, image.height);
        put16(26, 1);
        put16(28, 32);
        put32(30, 0);
        put32(34, static_cast<u32>(imageSize));

        const auto& from =
            order == ChannelOrder::Bgra ? SameOrder : SwappedRedBlue;
        const auto pixels = pack(image, 4, from);
        const auto rowSize = usize{image.width} * 4;

        auto file = openForWriting(path);
        file.write(reinterpret_cast<const char*>(header.data()), HeaderSize);
        for(u32 y = image.height; y-- > 0;) {
            file.write(
                reinterpret_cast<const char*>(pixels.data() + y * rowSize),
                static_cast<std::streamsize>(rowSize)
            );
        }
        finishWriting(file, path, true);
    }

    void writeImage(
        const std::filesystem::path& path,
        Rgba8View image,
        ChannelOrder order
    ) {
        if(path.extension() == ".png") {
            writePng(path, image, false, order);
            return;
        }

        writeBmp(path, image, order);
    }
}
