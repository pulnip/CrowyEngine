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

    // A flat quad. halfExtent.x spans along tangent and halfExtent.y along
    // the bitangent, cross(normal, tangent), so the quad may be rectangular.
    // normal and tangent must be orthonormal, and tangent is the direction of
    // increasing u as everywhere else, so the corner layout and the winding
    // match one face of MakeBox exactly.
    // uvScale tiles the texture per axis: 1 maps it once across the quad.
    MeshData MakePlane(
        Vec3 normal, Vec3 tangent,
        Vec2 halfExtent,
        Vec2 uvScale = {1.0f, 1.0f}
    );

    // The same quad lying in the XZ plane and facing +Y, i.e. a floor.
    MeshData MakePlane(Vec2 halfExtent, Vec2 uvScale = {1.0f, 1.0f});

    // square shorthands, since most planes are square
    MeshData MakePlane(Vec3 normal, Vec3 tangent, f32 halfSize, f32 uvScale = 1.0f);
    MeshData MakePlane(f32 halfSize = 1.0f, f32 uvScale = 1.0f);

    // A UV sphere with its poles on the Y axis.
    // v = 0 at the north pole (+Y) and grows southward; u wraps around +Y.
    // The seam is duplicated (slices+1 columns) so u does not jump inside a
    // triangle, and both poles collapse to a row of degenerate triangles,
    // which is the usual cost of this parameterization.
    MeshData MakeSphere(f32 radius = 1.0f, u32 slices = 32, u32 stacks = 16);
}
