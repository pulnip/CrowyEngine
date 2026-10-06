#include "PaintSceneQuery.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace Crowy
{
    void PaintSceneQuery::Rebuild(
        std::span<const PaintSurface> surfaces,
        std::span<const u8> active
    ) {
        triangles.clear();
        for(usize s = 0; s < surfaces.size(); ++s) {
            if(active[s] == 0)
                continue;
            const auto& mesh = surfaces[s].Mesh();
            const auto& t = surfaces[s].Object().transform;
            for(usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const auto ia = mesh.indices[i];
                const auto ib = mesh.indices[i + 1];
                const auto ic = mesh.indices[i + 2];
                Triangle tri{
                    .a = t.TransformPosition(toDVec3(mesh.positions[ia])),
                    .b = t.TransformPosition(toDVec3(mesh.positions[ib])),
                    .c = t.TransformPosition(toDVec3(mesh.positions[ic])),
                    .surface = s
                };
                auto n = cross(tri.b - tri.a, tri.c - tri.a);
                if(sizeSquared(n) == 0.0)
                    continue;
                const auto vertexNormal = toDVec3(mesh.normals[ia]) +
                                          toDVec3(mesh.normals[ib]) +
                                          toDVec3(mesh.normals[ic]);
                // the scale bends a normal the way an inverse transpose does
                const auto worldVertexNormal =
                    t.TransformVectorNoScale(vertexNormal / t.scale);
                if(dot(n, worldVertexNormal) < 0.0)
                    n = -n;
                tri.normal = getSafeNormal(n);
                triangles.push_back(tri);
            }
        }
    }

    std::optional<PaintHit> PaintSceneQuery::Raycast(
        DVec3 origin,
        DVec3 direction,
        f64 maxDistance,
        i64 onlySurface
    ) const {
        // a ray down a shared edge must hit one of its two triangles
        constexpr f64 EdgeSlack = 1e-9;
        const auto dir = getSafeNormal(direction);
        auto best = maxDistance;
        std::optional<PaintHit> hit;
        for(const auto& tri: triangles) {
            if(onlySurface >= 0 &&
               tri.surface != static_cast<usize>(onlySurface))
                continue;
            // Moller-Trumbore
            const auto e1 = tri.b - tri.a;
            const auto e2 = tri.c - tri.a;
            const auto p = cross(dir, e2);
            const auto det = dot(e1, p);
            if(std::abs(det) < 1e-12)
                continue;
            const auto inv = 1.0 / det;
            const auto s = origin - tri.a;
            const auto u = dot(s, p) * inv;
            if(u < -EdgeSlack || u > 1.0 + EdgeSlack)
                continue;
            const auto q = cross(s, e1);
            const auto v = dot(dir, q) * inv;
            if(v < -EdgeSlack || u + v > 1.0 + EdgeSlack)
                continue;
            const auto distance = dot(e2, q) * inv;
            if(distance <= 1e-6 || distance >= best)
                continue;

            best = distance;
            hit = PaintHit{
                .impactPoint = origin + dir * distance,
                .impactNormal = tri.normal,
                .surface = static_cast<i64>(tri.surface)
            };
        }

        return hit;
    }

    void PaintSceneQuery::OverlapSphere(
        DVec3 center,
        f64 radius,
        std::span<const PaintSurface> surfaces,
        std::span<const u8> active,
        std::vector<usize>& out
    ) {
        out.clear();
        for(usize s = 0; s < surfaces.size(); ++s) {
            if(active[s] == 0)
                continue;
            const auto& surface = surfaces[s];
            // the closest point of the scaled-local box to the centre
            const auto local =
                surface.Object().transform.InverseTransformPositionNoScale(
                    center
                );
            const auto bounds = surface.ScaledBounds();
            DVec3 closest;
            for(usize a = 0; a < 3; ++a)
                closest[a] = std::clamp(local[a], bounds.min[a], bounds.max[a]);
            if(sizeSquared(closest - local) <= radius * radius)
                out.push_back(s);
        }
    }
}
