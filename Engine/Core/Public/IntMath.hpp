#pragma once

#include <algorithm>
#include <bit>
#include <concepts>
#include <limits>

namespace Crowy
{
    template<std::unsigned_integral T>
    constexpr T nextPow2(T n) noexcept{
        constexpr auto BIT_WIDTH = std::numeric_limits<T>::digits;

        return n <= 1 ?
            T{1} : T{1} << (BIT_WIDTH - std::countl_zero(T(n-1)));
    }

    template<std::unsigned_integral T>
    constexpr T ceilDiv(T n, T d) noexcept{
        return (n + (d-1)) / d;
    }

    template<std::unsigned_integral T>
    constexpr T nextMul(T n, T m) noexcept{
        return ceilDiv(n, m) * m;
    }

    template<std::unsigned_integral T>
    T halve(T extent){
        return std::max<T>(1u, extent / 2);
    }
}
