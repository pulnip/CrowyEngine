#pragma once

#include <bit>

#include "Primitives.hpp"

// The CPU twin of Engine/Shader/WorldClock.slang: the world's steps of 1/60 s,
// every periodic thing looping over LoopSteps.
namespace Crowy
{
    // divides 2^32, so a step wrapped below 0 lands on the same phase
    inline constexpr u32 LoopSteps = 4096;

    // how far through its cycle a thing turning `cycles` times a loop is
    inline constexpr f32 loopPhase(u32 step, u32 cycles) {
        return static_cast<f32>((step * cycles) & (LoopSteps - 1)) /
               static_cast<f32>(LoopSteps);
    }

    static_assert(std::has_single_bit(LoopSteps));
    static_assert(loopPhase(0u - 3u, 5) == loopPhase(LoopSteps - 3, 5));
}
