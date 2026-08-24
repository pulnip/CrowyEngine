#pragma once

#include "Primitives.hpp"
#include "LinearAlgebra.hpp"
#include <concepts>
#include <type_traits>

namespace Crowy
{
    template<typename T>
    constexpr T abs(T) noexcept;

    template<typename T>
        requires std::integral<T> || std::is_floating_point_v<T>
    constexpr T abs(T t) noexcept{ return t < 0 ? -t : t; }

    template<>
    constexpr Vec2 abs<Vec2>(Vec2 v) noexcept{
        return Vec2{
            .x = abs(v.x),
            .y = abs(v.y)
        };
    }

    template<>
    constexpr Vec3 abs<Vec3>(Vec3 v) noexcept{
        return Vec3{
            .x = abs(v.x),
            .y = abs(v.y),
            .z = abs(v.z)
        };
    }

    template<>
    constexpr Vec4 abs<Vec4>(Vec4 v) noexcept{
        return Vec4{
            .x = abs(v.x),
            .y = abs(v.y),
            .z = abs(v.z),
            .w = abs(v.w)
        };
    }

    inline constexpr auto perp(const Vec2 v) noexcept{
        return Vec2{-v.y, v.x};
    }

    struct Projected{
        f32 center;
        f32 radius;
    };

    inline constexpr bool Overlap(
        Projected p0, Projected p1,
        const f32 epsilon = 0.0f
    ) noexcept{
        return overlap(
            p0.center - p0.radius, p0.center + p0.radius,
            p1.center - p1.radius, p1.center + p1.radius,
            epsilon
        );
    }
}
