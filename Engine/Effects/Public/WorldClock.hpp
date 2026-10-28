#pragma once

#include <bit>

#include "EffectConstants.h"
#include "Primitives.hpp"

// The CPU twin of WorldClock.slang: the world's steps of 1/60 s, every
// periodic thing looping over LoopSteps.
namespace Crowy
{
    inline constexpr u32 LoopSteps = CROWY_EFFECT_LOOP_STEPS;

    // how far through its cycle a thing turning `cycles` times a loop is
    inline constexpr f32 loopPhase(u32 step, u32 cycles) {
        return static_cast<f32>((step * cycles) & (LoopSteps - 1)) /
               static_cast<f32>(LoopSteps);
    }

    static_assert(std::has_single_bit(LoopSteps));
    static_assert(loopPhase(0u - 3u, 5) == loopPhase(LoopSteps - 3, 5));
}
