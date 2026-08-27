#pragma once

// The Keplerian solve, on the GPU.
//
// Mirrors Engine/RHI/Sample/Orbit/Kepler.cpp. The CPU keeps the clock and this
// only ever sees an angle: a mean anomaly is L0 + n*t - varpi, and by the time
// t is a few thousand days that expression has spent its float precision before
// Newton starts - Mercury's M passes a quarter of a million degrees inside one
// ring. So time arrives as 32-bit fixed point in turns, where one turn is 2^32
// and uint overflow is the reduction mod a full circle.

static const uint ORBIT_PHASE_BLOCK_SHIFT = 8;

// Mirrors OrbitElementsGPU in Kepler.hpp. The three rotations arrive as sine
// and cosine because they are constant per body; rebuilding them per sample
// would be six transcendentals spent on a constant.
struct OrbitElements{
    float a;
    float e;
    // argument of perihelion, varpi - Omega
    float cosPeri, sinPeri;
    float cosInc, sinInc;
    float cosNode, sinNode;
};

// Mirrors OrbitPhaseGPU in Kepler.hpp. Splitting the walk into blocks keeps
// both products short, so perStep's half-ulp rounding cannot pile up across a
// whole ring - see the comment there for the numbers.
struct OrbitPhase{
    uint phase0;
    uint perStep;
    uint perBlock;
    uint _pad0;
};

// radians, reduced to (-pi, pi]
float orbitMeanAnomaly(OrbitPhase phase, uint index){
    const uint fixed = phase.phase0 +
        phase.perBlock * (index >> ORBIT_PHASE_BLOCK_SHIFT) +
        phase.perStep * (index & ((1u << ORBIT_PHASE_BLOCK_SHIFT) - 1u));

    // reinterpreting as signed lands it in (-0.5, 0.5] turns, which halves the
    // magnitude float() has to carry and is where Newton wants to start
    return float(int(fixed)) * (6.28318530717958648 / 4294967296.0);
}

// Newton on E - e*sin(E) = M.
//
// A fixed four steps rather than a convergence test: the CPU solver needs three
// at Mercury's eccentricity and this has no way to report a miss, so it spends
// one more and keeps the wavefront branchless.
float orbitSolveEccentric(float meanAnomaly, float e){
    float E = meanAnomaly + e * sin(meanAnomaly);

    [unroll]
    for(int i = 0; i < 4; ++i){
        // 1 - e*cos(E) is the derivative and never vanishes for e < 1
        E -= (E - e * sin(E) - meanAnomaly) / (1.0 - e * cos(E));
    }

    return E;
}

// heliocentric ecliptic position in AU, for the sample `index` steps after the
// phase's epoch
float3 orbitKeplerPosition(OrbitElements el, OrbitPhase phase, uint index){
    // the Sun row: no orbit in this frame, and its trail being exactly zero is
    // what lets the frame interpolation skip a special case
    if(el.a <= 0.0)
        return float3(0.0, 0.0, 0.0);

    const float E = orbitSolveEccentric(orbitMeanAnomaly(phase, index), el.e);

    // in the orbital plane, perihelion on +x
    const float xOrb = el.a * (cos(E) - el.e);
    const float yOrb = el.a * sqrt(1.0 - el.e * el.e) * sin(E);

    // Rz(Omega) * Rx(i) * Rz(varpi - Omega), applied in that order
    const float xNode = xOrb * el.cosPeri - yOrb * el.sinPeri;
    const float yNode = xOrb * el.sinPeri + yOrb * el.cosPeri;

    const float yIncl = yNode * el.cosInc;
    const float zIncl = yNode * el.sinInc;

    return float3(
        xNode * el.cosNode - yIncl * el.sinNode,
        xNode * el.sinNode + yIncl * el.cosNode,
        zIncl
    );
}
