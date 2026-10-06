#pragma once

#include <optional>
#include <span>
#include <vector>

#include "MintFrame.hpp"
#include "PaintBrushProfile.hpp"
#include "PaintSurface.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // A line trace and a sphere overlap over the shown surfaces, standing in
    // for Unreal's Visibility channel: every triangle in world cm, brute force
    class PaintSceneQuery {
    private:
        struct Triangle {
            DVec3 a;
            DVec3 b;
            DVec3 c;
            // outward, as the vertex normals say
            DVec3 normal;
            usize surface = 0;
        };

        std::vector<Triangle> triangles;

    public:
        void Rebuild(
            std::span<const PaintSurface> surfaces,
            std::span<const u8> active
        );

        // the nearest front or back face along the ray, within maxDistance;
        // `onlySurface` keeps the trace to one surface
        std::optional<PaintHit> Raycast(
            DVec3 origin,
            DVec3 direction,
            f64 maxDistance,
            i64 onlySurface = -1
        ) const;

        // the shown surfaces whose box the sphere reaches
        static void OverlapSphere(
            DVec3 center,
            f64 radius,
            std::span<const PaintSurface> surfaces,
            std::span<const u8> active,
            std::vector<usize>& out
        );
    };
}
