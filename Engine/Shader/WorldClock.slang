#pragma once

// The world's clock in steps of 1/60 s; every periodic thing loops over
// LoopSteps, which divides 2^32, so a step wrapped below 0 still agrees.

static const uint LoopSteps = 4096u;

// how far through its cycle a thing turning `cycles` times a loop is
float loopPhase(uint step, uint cycles) {
    return float((step * cycles) & (LoopSteps - 1u)) / float(LoopSteps);
}
