#pragma once

#include <string>

namespace Crowy
{
    enum class RenderType{
        Opaque,
        Transparent,
        Unlit,
    };

    RenderType toRenderType(const std::string& text);
}