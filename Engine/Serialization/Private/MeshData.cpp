#include "MeshData.hpp"

#include <algorithm>

#include "Geometry/Overlap3D.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    AABB3D boundsOf(std::span<const Vertex> vertices) {
        if(vertices.empty())
            return AABB3D{.center = zeros(), .halfScale = zeros()};

        auto low = vertices.front().position;
        auto high = low;
        for(const auto& vertex: vertices) {
            const auto& p = vertex.position;
            low = Vec3{
                std::min(low.x, p.x),
                std::min(low.y, p.y),
                std::min(low.z, p.z)
            };
            high = Vec3{
                std::max(high.x, p.x),
                std::max(high.y, p.y),
                std::max(high.z, p.z)
            };
        }

        return AABB3D{
            .center = 0.5f * (low + high),
            .halfScale = 0.5f * (high - low)
        };
    }
}
