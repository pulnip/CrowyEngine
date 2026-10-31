#pragma once

#include <algorithm>
#include <concepts>
#include <type_traits>

#include "LinearAlgebra.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    template<typename T>
    constexpr T abs(T) noexcept;

    template<typename T>
        requires std::integral<T> || std::is_floating_point_v<T>
    constexpr T abs(T t) noexcept {
        return t < 0 ? -t : t;
    }

    template<>
    constexpr Vec2 abs<Vec2>(Vec2 v) noexcept {
        return Vec2{.x = abs(v.x), .y = abs(v.y)};
    }

    template<>
    constexpr Vec3 abs<Vec3>(Vec3 v) noexcept {
        return Vec3{.x = abs(v.x), .y = abs(v.y), .z = abs(v.z)};
    }

    template<>
    constexpr Vec4 abs<Vec4>(Vec4 v) noexcept {
        return Vec4{.x = abs(v.x), .y = abs(v.y), .z = abs(v.z), .w = abs(v.w)};
    }

    inline f32 distanceToSegment(Vec2 p, Vec2 a, Vec2 b) noexcept {
        const auto along = b - a;
        const auto length = normSquared(along);
        const auto t = length > 0.0f
                           ? std::clamp(dot(p - a, along) / length, 0.0f, 1.0f)
                           : 0.0f;

        return norm(p - (a + along * t));
    }
}
