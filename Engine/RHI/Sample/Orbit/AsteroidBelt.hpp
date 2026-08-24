#pragma once

#include <span>
#include <vector>
#include "Kepler.hpp"

namespace Crowy
{
    // A synthetic main belt between Mars and Jupiter.
    //
    // The elements are made up, not catalogued: the point is to fill the gap
    // the planets leave with something that moves correctly, so the picture
    // reads as a system rather than nine curves. Every asteroid is a plain
    // Keplerian orbit solved by the same code the planets use, so at alpha 0
    // the whole belt breaks into epicycles along with everything else.
    //
    // No trails - a few thousand overlapping ones would be a grey smear. Only
    // the current position of each, drawn as a dot.

    inline constexpr u32 ASTEROID_COUNT = 2400;

    // where the belt actually lives, in AU
    inline constexpr f64 BELT_INNER_AU = 2.06;
    inline constexpr f64 BELT_OUTER_AU = 3.40;

    class AsteroidBelt{
    private:
        std::vector<OrbitalElements> elements;

    public:
        // `seed` picks the swarm; the same seed always gives the same belt, so
        // a capture is reproducible
        explicit AsteroidBelt(u32 count = ASTEROID_COUNT, u32 seed = 0x5EED4A57u);

        usize Count() const noexcept{ return elements.size(); }
        const OrbitalElements& At(usize i) const{ return elements[i]; }
        std::span<const OrbitalElements> Elements() const noexcept{
            return elements;
        }

        // heliocentric positions at `day`, one per asteroid
        void Sample(f64 day, std::span<Vec3> out) const;
    };
}
