#pragma once

#include "MeshData.hpp"
#include "Primitives.hpp"

// The Island's own meshes, centered at the origin as MeshGenerator's are and
// following MeshData's convention.
namespace Crowy
{
    // a unit sphere stretched to `radii` with the stretch's own normals: the
    // vertex stage turns a normal by the world's rotation alone
    MeshData makeEllipsoid(Vec3 radii);

    // a capped cylinder along y, `halfLength` each way
    MeshData makeCylinder(f32 radius, f32 halfLength, u32 segments);

    // the tipi's canvas, flat per facet, its first facets left open, centered
    // halfway between its base and its top; its fronts face outward
    MeshData makeTipiCanvas();
}
