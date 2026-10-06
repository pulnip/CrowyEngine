#pragma once

#include <bit>
#include <cmath>

#include "Primitives.hpp"

// Unreal's FRandomStream and HashCombineFast, bit for bit: the splat's seeded
// rotation and the splash's droplets come out as they do in MintChoco
namespace Crowy
{
    // FRandomStream: the seed mutates before every draw
    class RandomStream {
    private:
        u32 seed = 0;

    public:
        explicit constexpr RandomStream(i32 initial) noexcept
            : seed(static_cast<u32>(initial)) {}

        // GetFraction: [0, 1) from the mantissa bits of the mutated seed
        f32 FRand() noexcept {
            seed = seed * 196314165u + 907633515u;

            return std::bit_cast<f32>(0x3F800000u | (seed >> 9)) - 1.0f;
        }

        f32 FRandRange(f32 low, f32 high) noexcept {
            return low + (high - low) * FRand();
        }

        u32 Seed() const noexcept { return seed; }
    };

    inline constexpr u32 hashCombineFast(u32 a, u32 b) noexcept {
        return a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
    }

    // FMath::RoundToInt
    inline i32 roundToInt(f32 x) noexcept {
        return static_cast<i32>(std::floor(x + 0.5f));
    }
}
