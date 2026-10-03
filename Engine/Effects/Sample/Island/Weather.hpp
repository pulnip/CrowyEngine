#pragma once

#include <cmath>
#include <numbers>

#include "IslandScene.h"
#include "LinearAlgebra.hpp"
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

    // the sine of a phase in cycles, its argument kept in [-pi, pi)
    inline f32 cycleSine(f32 cycles) {
        constexpr auto Pi = std::numbers::pi_v<f32>;

        const auto c = cycles - std::floor(cycles);
        return std::sin(2.0f * Pi * c - Pi);
    }

    // the wind at xz in m/s: the mean, gust fronts carried downwind, and a
    // crosswind that wanders
    inline Vec2 windAt(Vec2 xz, u32 step) {
        const Vec2 mean{ISLAND_WIND};
        const auto speed = std::sqrt(dot(mean, mean));
        const auto along = mean / speed;
        const Vec2 across{-along.y, along.x};
        // the second front comes in 35 deg off the first
        const Vec2 turned{
            along.x * 0.819152f - along.y * 0.573576f,
            along.x * 0.573576f + along.y * 0.819152f
        };

        const auto front =
            0.6f * cycleSine(dot(xz, along) / 7.0f - loopPhase(step, 11)) +
            0.4f * cycleSine(dot(xz, turned) / 4.3f - loopPhase(step, 17));
        auto gust = std::max(front, 0.0f);
        gust *= std::sqrt(gust);
        const auto swell = 0.6f + 0.4f * cycleSine(loopPhase(step, 4));
        const auto wander =
            0.25f * speed *
            cycleSine(dot(xz, along) / 5.0f - loopPhase(step, 13));

        return along * (speed + swell * ISLAND_GUST * gust) + across * wander;
    }
}
