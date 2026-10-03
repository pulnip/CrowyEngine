#pragma once

// The world's rhythms as functions of the world's step, read alike by the
// effects and the scene: the fire's breath, the wind.

#include "IslandScene.h"
#include "WorldClock.slang"

// how strongly the fire burns this step, around 1
float fireBreath(uint step) {
    float slow = sin(6.2831853 * loopPhase(step, ISLAND_BREATH_SLOW));
    float fast = sin(6.2831853 * loopPhase(step, ISLAND_BREATH_FAST));
    return 1.0 + ISLAND_BREATH_SLOW_DEPTH * slow +
           ISLAND_BREATH_FAST_DEPTH * fast;
}

// the sine of a phase in cycles, its argument kept in [-pi, pi) where GPU
// sines are tightest
float cycleSine(float cycles) {
    float c = cycles - floor(cycles);
    return sin(6.2831853 * c - 3.1415927);
}

// the wind at xz in m/s: the mean, gust fronts carried downwind, and a
// crosswind that wanders
float2 windAt(float2 xz, uint step) {
    float2 mean = float2(ISLAND_WIND);
    float speed = length(mean);
    float2 along = mean / speed;
    float2 across = float2(-along.y, along.x);
    // the second front comes in 35 deg off the first
    float2 turned = float2(
        along.x * 0.819152 - along.y * 0.573576,
        along.x * 0.573576 + along.y * 0.819152
    );

    float front =
        0.6 * cycleSine(dot(xz, along) / 7.0 - loopPhase(step, 11u)) +
        0.4 * cycleSine(dot(xz, turned) / 4.3 - loopPhase(step, 17u));
    float gust = max(front, 0.0);
    gust *= sqrt(gust);
    float swell = 0.6 + 0.4 * cycleSine(loopPhase(step, 4u));
    float wander =
        0.25 * speed * cycleSine(dot(xz, along) / 5.0 - loopPhase(step, 13u));
    return along * (speed + swell * ISLAND_GUST * gust) + across * wander;
}
