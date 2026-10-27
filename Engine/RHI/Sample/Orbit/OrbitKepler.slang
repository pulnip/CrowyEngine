#pragma once

// Mirrors Kepler.cpp, but time arrives as fixed-point turns (one turn is 2^32):
// a float mean anomaly runs out of precision within one ring.

static const uint ORBIT_PHASE_BLOCK_SHIFT = 8;

// Mirrors OrbitElementsGPU in Kepler.hpp.
struct OrbitElements{
    float a;
    float e;
    // argument of perihelion, varpi - Omega
    float cosPeri, sinPeri;
    float cosInc, sinInc;
    float cosNode, sinNode;
};

// Mirrors OrbitPhaseGPU in Kepler.hpp.
struct OrbitPhase{
    uint phase0;
    uint perStep;
    uint perBlock;
    uint _pad0;
};

float orbitMeanAnomaly(OrbitPhase phase, uint index){
    const uint fixed = phase.phase0 +
        phase.perBlock * (index >> ORBIT_PHASE_BLOCK_SHIFT) +
        phase.perStep * (index & ((1u << ORBIT_PHASE_BLOCK_SHIFT) - 1u));

    // read as signed it lies within half a turn, which halves what float()
    // carries and is where Newton wants to start
    return float(int(fixed)) * (6.28318530717958648 / 4294967296.0);
}

// Four fixed steps, one more than the CPU needs at Mercury: this cannot report
// a miss, and no convergence test keeps the wavefront branchless.
float orbitSolveEccentric(float meanAnomaly, float e){
    float E = meanAnomaly + e * sin(meanAnomaly);

    [unroll]
    for(int i = 0; i < 4; ++i){
        // 1 - e*cos(E) is the derivative and never vanishes for e < 1
        E -= (E - e * sin(E) - meanAnomaly) / (1.0 - e * cos(E));
    }

    return E;
}

float3 orbitKeplerPosition(OrbitElements el, OrbitPhase phase, uint index){
    // an exactly-zero Sun lets the frame interpolation skip a special case
    if(el.a <= 0.0)
        return float3(0.0, 0.0, 0.0);

    const float E = orbitSolveEccentric(orbitMeanAnomaly(phase, index), el.e);

    const float xOrb = el.a * (cos(E) - el.e);
    const float yOrb = el.a * sqrt(1.0 - el.e * el.e) * sin(E);

    // Rz(Omega) * Rx(i) * Rz(varpi - Omega)
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
