#include <fstream>
#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "Log.hpp"
#include "PathUtils.hpp"
#include "TextureImporter.hpp"

namespace Crowy
{
    std::optional<TextureData> importTexture(const std::filesystem::path& path){
        auto resolvedPath = resolveAssetPath(path);

        std::ifstream file(resolvedPath, std::ios::binary | std::ios::ate);
        if(!file) return std::nullopt;

        auto size = file.tellg();
        file.seekg(0);
        std::vector<uint8_t> buffer(size);
        file.read(reinterpret_cast<char*>(buffer.data()), size);

        int width, height, channels;
        // Force RGBA output (4 channels)
        uint8_t* data = stbi_load_from_memory(
            buffer.data(), static_cast<int>(buffer.size()),
            &width, &height, &channels, STBI_rgb_alpha
        );

        if(!data){
            LOG_ERROR(LOG_RESOURCE, "Failed to load image: {} - {}",
                path, stbi_failure_reason());
            return std::nullopt;
        }

        LOG_DEBUG(LOG_RESOURCE, "Loaded image: {} ({}x{}, {} channels)",
            path, width, height, channels);

        TextureData result;
        size_t dataSize = static_cast<size_t>(width) * height * 4; // 4 channels (RGBA)
        result.pixels.resize(dataSize);
        std::memcpy(result.pixels.data(), data, dataSize);
        result.width = width;
        result.height = height;
        result.channels = 4; // forced RGBA

        stbi_image_free(data);

        return result;
    }
}
