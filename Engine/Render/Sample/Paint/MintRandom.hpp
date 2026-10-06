#pragma once

#include <bit>

#include "Primitives.hpp"

// Unreal's FRandomStream and HashCombineFast, bit for bit: the splat's seeded
// rotation and the splash's droplets come out as they do in MintChoco
namespace Crowy
{
    inline constexpr u32 hashCombineFast(u32 a, u32 b) noexcept {
        return a ^ (b + 0x9e3779b9u + (a << 6) + (a >> 2));
    }

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

        // in double, as FVector::FReal; callers narrow as MintChoco's do
        f64 FRandRange(f64 low, f64 high) noexcept {
            return low + (high - low) * static_cast<f64>(FRand());
        }

        u32 Seed() const noexcept { return seed; }
    };
}
