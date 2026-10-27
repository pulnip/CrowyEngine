#pragma once

#include <filesystem>

#include "ImageView.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    enum class ChannelOrder : u8 { Rgba, Bgra };

    // RGB unless alpha: the back buffer's alpha is arbitrary and never
    // compared. Throws std::runtime_error when the file cannot be written.
    void writePng(
        const std::filesystem::path& path,
        Rgba8View image,
        bool alpha = false,
        ChannelOrder order = ChannelOrder::Rgba
    );
    void writeBmp(
        const std::filesystem::path& path,
        Rgba8View image,
        ChannelOrder order = ChannelOrder::Rgba
    );
    // PNG for a .png path, BMP for any other
    void writeImage(
        const std::filesystem::path& path,
        Rgba8View image,
        ChannelOrder order = ChannelOrder::Rgba
    );
}
