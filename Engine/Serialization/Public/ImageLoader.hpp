#pragma once

#include <filesystem>
#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    struct ImageData{
        std::vector<u8> blob;
        std::vector<RHISubresourceData> subs;
        RHIPixelFormat format = RHIPixelFormat::Unknown;
        u32 width = 0, height = 0;
        u32 mipLevels = 1, arraySize = 1;
    };

    ImageData LoadImage(const std::filesystem::path&);
}
