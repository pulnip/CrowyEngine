#pragma once

// The world's clock in steps of 1/60 s; every periodic thing loops over
// LoopSteps.

#include "EffectConstants.h"

static const uint LoopSteps = CROWY_EFFECT_LOOP_STEPS;

// how far through its cycle a thing turning `cycles` times a loop is
float loopPhase(uint step, uint cycles) {
    return float((step * cycles) & (LoopSteps - 1u)) / float(LoopSteps);
}
