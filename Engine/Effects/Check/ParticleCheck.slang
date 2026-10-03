// A particle whose whole life the CPU can replay: its life from the hash,
// its position counting what happened to it.

#include "Effect.slang"

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    p.lifeSteps = 20u + effectHash(effect.seed, slot, generation, 1u) % 40u;
    p.generation = generation;
    p.position = float3(float(slot), float(generation), 0.0);
    return p;
}

void update(inout Particle p, uint slot) {
    p.position.z += 1.0;
}

#include "ParticleKernel.slang"
