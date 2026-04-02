#pragma once

#include <span>
#include "math.hpp"
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"

namespace Crowy
{
    struct RenderItem{
        MeshHandle mesh;
        MaterialSetHandle materials;
        Mat4 world;
        RenderTypeHash type;
    };

    struct RenderContext{
        std::span<const RenderItem> renderItems;
        // Camera Information
        Mat4 view, proj;
    };
}