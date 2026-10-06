#pragma once

#include <vector>

#include "MintFrame.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // FPaintMeshTriangles: one slot's triangles in mesh-local cm; a triangle
    // faces out when cross(b - a, c - a) does, as MeshData's winding wants
    struct PaintMeshTriangles {
        std::vector<Vec3> positions;
        // one per position; they only settle which way a triangle faces
        std::vector<Vec3> normals;
        std::vector<u32> indices;
    };

    Box3d boundsOf(const PaintMeshTriangles& mesh);

    // flat-shaded, centred on the origin, `size` cm along every edge
    PaintMeshTriangles makePaintCube(f64 size);

    // a ramp over a size.x by size.y footprint centred on the origin, rising
    // from 0 at -x to size.z at +x; the high end is a vertical +X face
    PaintMeshTriangles makePaintWedge(DVec3 size);

    // smooth-shaded, poles on Z, centred on the origin
    PaintMeshTriangles makePaintSphere(f64 radius, u32 slices, u32 stacks);
}
