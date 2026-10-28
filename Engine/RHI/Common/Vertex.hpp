#pragma once

#include <type_traits>

#include "Primitives.hpp"

namespace Crowy
{
    struct Vertex {
        Vec3 position;
        Vec3 normal;
        Vec2 texCoord;
        // xyz runs along increasing u; w is the handedness, so increasing v
        // runs along w * cross(normal, tangent.xyz)
        Vec4 tangent;
    };
    static_assert(sizeof(Vertex) == 48, "Vertex should be 48 bytes");
    static_assert(
        std::is_trivially_copyable_v<Vertex>,
        "Vertex must be trivially copyable"
    );
}
