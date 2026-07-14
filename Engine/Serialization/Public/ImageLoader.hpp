#pragma once

#include <filesystem>
#include <vector>
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    enum class ImageFormat{
        RGBA
    };
    u32 toChannels(ImageFormat) noexcept;

    using ImageBlob = std::vector<u8>;

    class ImageData final{
    private:
        ImageBlob buffer;
        u32 width = 0, height = 0;
        ImageFormat format = ImageFormat::RGBA;

    public:
        ImageData() = default;
        ~ImageData() = default;
        CROWY_DECLARE_TRANSFERABLE(ImageData)

        ImageData(const ImageBlob& buffer, u32 width, u32 height, ImageFormat format)
            : buffer(buffer), width(width), height(height), format(format) {}
        ImageData(ImageBlob&& buffer, u32 width, u32 height, ImageFormat format)
            : buffer(std::move(buffer)), width(width), height(height), format(format) {}

        inline bool IsValid() const noexcept{
            return !buffer.empty() &&
                width > 0 &&
                height > 0;
        }

        inline u32 GetWidth() const noexcept{ return width; }
        inline u32 GetHeight() const noexcept{ return height; }
        inline u32 GetChannels() const noexcept{ return toChannels(format); }

        inline auto GetBufferPointer(this auto& self) noexcept{
            return self.buffer.data();
        }
        inline usize GetBufferSize() const noexcept{
            return buffer.size();
        }
    };

    ImageData loadImage(const std::filesystem::path&,
        // Desired Channels = RGBA(4)
        ImageFormat desiredFormat = ImageFormat::RGBA
    );
}
