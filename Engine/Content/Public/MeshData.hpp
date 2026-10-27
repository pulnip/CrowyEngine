#pragma once

#include <span>
#include <vector>

#include "Primitives.hpp"
#include "Vertex.hpp"

namespace Crowy
{
    struct AABB3D;

    // left-handed, +Y up, fronts wind clockwise seen from outside (the RHI's
    // default winding), texCoord (0, 0) at the top-left
    struct MeshData {
        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    AABB3D boundsOf(std::span<const Vertex> vertices);
}
