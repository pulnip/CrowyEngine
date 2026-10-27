#pragma once

#include "MeshData.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // every face owns its four vertices, so normals stay hard across edges
    MeshData MakeBox(f32 halfSize = 1.0f);

    // halfExtent.x along tangent (increasing u), .y along cross(normal,
    // tangent); orthonormal axes give the corners and winding of a MakeBox face
    MeshData MakePlane(
        Vec3 normal,
        Vec3 tangent,
        Vec2 halfExtent,
        Vec2 uvScale = {1.0f, 1.0f}
    );

    // in the XZ plane, facing +Y
    MeshData MakePlane(Vec2 halfExtent, Vec2 uvScale = {1.0f, 1.0f});

    MeshData MakePlane(
        Vec3 normal,
        Vec3 tangent,
        f32 halfSize,
        f32 uvScale = 1.0f
    );
    MeshData MakePlane(f32 halfSize = 1.0f, f32 uvScale = 1.0f);

    // v = 0 at the north pole (+Y), u wraps around +Y; the seam is duplicated
    // (slices + 1 columns) so u never jumps inside a triangle
    MeshData MakeSphere(f32 radius = 1.0f, u32 slices = 32, u32 stacks = 16);
}
