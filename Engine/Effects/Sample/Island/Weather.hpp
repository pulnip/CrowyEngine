#pragma once

#include <cmath>
#include <numbers>

#include "IslandScene.h"
#include "Primitives.hpp"
#include "WorldClock.hpp"

// The CPU twin of Weather.slang: the world's rhythms by the world's step.
namespace Crowy
{
    // how strongly the fire burns this step, around 1
    inline f32 fireBreath(u32 step) {
        constexpr auto TwoPi = 2.0f * std::numbers::pi_v<f32>;

        const auto slow = std::sin(TwoPi * loopPhase(step, ISLAND_BREATH_SLOW));
        const auto fast = std::sin(TwoPi * loopPhase(step, ISLAND_BREATH_FAST));

        return 1.0f + ISLAND_BREATH_SLOW_DEPTH * slow +
               ISLAND_BREATH_FAST_DEPTH * fast;
    }
}
