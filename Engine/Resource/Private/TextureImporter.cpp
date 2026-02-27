#include <vector>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "path_util.hpp"
#include "string.hpp"
#include "Log.hpp"
#include "SchemeKind.hpp"
#include "TextureImporter.hpp"

namespace Crowy
{
    static std::optional<TextureData> loadTexture(const std::string& path){
        auto resolvedPath = get_absolute_path(to_path(path.c_str()));

        auto buffer = readFileAsBinary(resolvedPath);

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

    static std::optional<TextureData> loadEmbeddedTexture(const std::string& name){
        return TextureData{
            .pixels = {0xFF, 0x00, 0x00, 0xFF},
            .width = 1,
            .height = 1,
            .channels = 4
        };
    }

    std::optional<TextureData> importTexture(const std::string& uri){
        auto [scheme, path] = splitSchemeAndPath(uri);

        switch(scheme){
        case SchemeKind::File:
            return loadTexture(path);
        case SchemeKind::Embedded:
            return loadEmbeddedTexture(path);
        case SchemeKind::Unknown:
            return std::nullopt;
        default:
            std::unreachable();
        }
    }
}
