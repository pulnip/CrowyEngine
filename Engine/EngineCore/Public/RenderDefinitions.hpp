#pragma once

#include <string>
#include <vector>
#include "math.hpp"
#include "ResourceHandle.hpp"

namespace Crowy
{
    using RenderType = std::string;
    using RenderTypeHash = std::invoke_result_t<std::hash<RenderType>, RenderType>;

    struct RenderItem{
        MeshHandle mesh;
        MaterialSetHandle materials;
        Mat4 world;
        RenderType type;
    };
    using RenderQueue = std::vector<RenderItem>;
}