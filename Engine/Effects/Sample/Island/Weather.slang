#pragma once

// The world's rhythms as functions of the world's step, read alike by the
// effects and the scene: the fire's breath.

#include "IslandScene.h"
#include "WorldClock.slang"

// how strongly the fire burns this step, around 1
float fireBreath(uint step) {
    float slow = sin(6.2831853 * loopPhase(step, ISLAND_BREATH_SLOW));
    float fast = sin(6.2831853 * loopPhase(step, ISLAND_BREATH_FAST));
    return 1.0 + ISLAND_BREATH_SLOW_DEPTH * slow +
           ISLAND_BREATH_FAST_DEPTH * fast;
}
