#pragma once

#include "MeshData.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // Procedural meshes, all centered at the origin and following the
    // convention documented on MeshData.

    // An axis-aligned cube with flat shading: every face owns its 4 vertices,
    // for 24 vertices and 36 indices, so normals stay hard across the edges.
    MeshData MakeBox(f32 halfSize = 1.0f);

    // A UV sphere with its poles on the Y axis.
    // v = 0 at the north pole (+Y) and grows southward; u wraps around +Y.
    // The seam is duplicated (slices+1 columns) so u does not jump inside a
    // triangle, and both poles collapse to a row of degenerate triangles,
    // which is the usual cost of this parameterization.
    MeshData MakeSphere(f32 radius = 1.0f, u32 slices = 32, u32 stacks = 16);
}
