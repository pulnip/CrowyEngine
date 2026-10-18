#pragma once

#include <span>
#include <vector>

#include "Primitives.hpp"
#include "Vertex.hpp"

namespace Crowy
{
    struct AABB3D;

    // Geometry convention every MeshData in this module follows,
    // unless a producer documents otherwise:
    // - left-handed coordinate system, +Y up.
    // - a triangle faces front when its winding is clockwise as seen from
    //   outside the mesh, so a pipeline consuming this data leaves
    //   RHIRasterizerState::frontCounterClockwise at false.
    //   viewing a mesh from the inside, as a skybox does, means reversing
    //   the index order or flipping the cull mode at the call site.
    // - texCoord follows the D3D convention, with (0, 0) at the top-left texel.
    struct MeshData{
        std::vector<Vertex> vertices;
        std::vector<u32> indices;
    };

    // the tightest box around the positions; an empty one for no vertices
    AABB3D boundsOf(std::span<const Vertex> vertices);
}
