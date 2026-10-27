#pragma once

#include <span>
#include <vector>

#include "Kepler.hpp"

namespace Crowy
{
    // Made-up elements, not catalogued ones: the belt only has to fill the gap
    // between Mars and Jupiter with orbits that move correctly.
    inline constexpr u32 ASTEROID_COUNT = 2400;

    inline constexpr f64 BELT_INNER_AU = 2.06;
    inline constexpr f64 BELT_OUTER_AU = 3.40;

    class AsteroidBelt {
    private:
        std::vector<OrbitalElements> elements;

    public:
        explicit AsteroidBelt(
            u32 count = ASTEROID_COUNT,
            u32 seed = 0x5EED4A57u
        );

        usize Count() const noexcept { return elements.size(); }
        const OrbitalElements& At(usize i) const { return elements[i]; }
        std::span<const OrbitalElements> Elements() const noexcept {
            return elements;
        }

        void Sample(f64 day, std::span<Vec3> out) const;
    };
}
