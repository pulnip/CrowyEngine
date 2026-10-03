#pragma once

// The sea as a height field of the world's step: sharp-crested waves
// running with the wind, quieter over the island's shallows.

#include "IslandShapes.slang"
#include "SeaWaves.h"
#include "WorldClock.slang"

static const float SeaInvLength[SEA_WAVE_COUNT] = {SEA_INV_LENGTHS};
static const float SeaAmplitude[SEA_WAVE_COUNT] = {SEA_AMPLITUDES};
static const float SeaDirectionX[SEA_WAVE_COUNT] = {SEA_DIRECTIONS_X};
static const float SeaDirectionZ[SEA_WAVE_COUNT] = {SEA_DIRECTIONS_Z};
static const uint SeaCycles[SEA_WAVE_COUNT] = {SEA_CYCLES};
static const float SeaPhase[SEA_WAVE_COUNT] = {SEA_PHASES};

// how much of a wave the polar grid can carry at radius r: its cells grow
// with r, and a wave under four of them would alias
float seaCellFade(float r, float invLength) {
    float cell = max(r, 3.5) * 6.2831853 / 256.0;
    return saturate(1.0 / (invLength * 4.0 * cell) - 1.0);
}

// the waves damp toward the waterline, where the still water is shallow
float seaShore(float2 xz) {
    return lerp(0.2, 1.0, smoothstep(0.0, 1.2, -islandCrown(xz)));
}

// the first `count` waves at xz; `onGrid` fades those the grid cannot carry
float seaHeight(float2 xz, uint step, uint count, bool onGrid) {
    float shore = seaShore(xz);
    float r = length(xz);
    float2 p = xz;
    float height = 0.0;
    for(uint i = 0u; i < count; ++i) {
        float amplitude = SeaAmplitude[i] * shore;
        if(onGrid)
            amplitude *= seaCellFade(r, SeaInvLength[i]);
        float2 direction = float2(SeaDirectionX[i], SeaDirectionZ[i]);
        float c = dot(direction, p) * SeaInvLength[i] -
                  loopPhase(step, SeaCycles[i]) + SeaPhase[i];
        c -= floor(c);
        float angle = 6.2831853 * c - 3.1415927;
        float crest = exp(sin(angle) - 1.0);
        height += amplitude * (crest - SEA_MEAN);
        // the shorter waves after it gather on its crests
        float drag = SEA_DRAG * amplitude / SeaAmplitude[0] /
                     (SeaInvLength[0] * 6.2831853);
        p -= direction * (crest * cos(angle) * drag);
    }
    return height;
}
