#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace Crowy
{
    // Raw image data loaded from file
    struct TextureData{
        std::vector<uint8_t> pixels;
        int width = 0;
        int height = 0;
        int channels = 0;

        inline uint32_t getWidth() const{
            return static_cast<uint32_t>(width);
        }
        inline uint32_t getHeight() const{
            return static_cast<uint32_t>(height);
        }

        inline bool isValid() const{
            return !pixels.empty() &&
            width > 0 && height > 0;
        }
        inline size_t dataSize() const{
            return pixels.size();
        }
    };

    // Load image from file (supports PNG, JPG, BMP, TGA, etc.)
    // Returns RGBA data (4 channels)
    std::optional<TextureData> importTexture(const std::filesystem::path& path);
}
