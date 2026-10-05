#pragma once

#include "Primitives.hpp"

// The CPU twin of Engine/Shader/Random.slang, bit for bit, so a check can
// hold the GPU's draws against it.
namespace Crowy
{
    // PCG's output permutation over 32 bits (Jarzynski and Olano, 2020)
    inline constexpr u32 pcgHash(u32 value) {
        const auto state = value * 747796405u + 2891336453u;
        const auto word =
            ((state >> ((state >> 28u) + 4u)) ^ state) * 277803737u;

        return (word >> 22u) ^ word;
    }

    // one draw of an effect's particle: its seed, slot, generation and the
    // draw's index within that particle's life
    inline constexpr u32 effectHash(u32 seed, u32 slot, u32 generation, u32 k) {
        return pcgHash(seed ^ pcgHash(slot ^ pcgHash(generation ^ pcgHash(k))));
    }

    // effectHash's top 24 bits as a uniform in [0, 1)
    inline constexpr f32 effectRandom(
        u32 seed,
        u32 slot,
        u32 generation,
        u32 k
    ) {
        return static_cast<f32>(effectHash(seed, slot, generation, k) >> 8u) *
               (1.0f / 16777216.0f);
    }

    static_assert(pcgHash(0) == 0x07bb2fe2u);
    static_assert(effectHash(1, 0, 0, 0) == 0x01443b47u);
}
