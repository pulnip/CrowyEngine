// Probes of the weather for ParticleCheck: each slot evaluates the wind and
// the sea at its own place and world step, for the CPU to hold against.

#include "Effect.slang"
#include "Sea.slang"
#include "Weather.slang"

static const uint ProbeSide = 64u;
// the world steps probed: wrapped ones below 0 and the loop's edge among them
static const uint ProbeSteps[10] = {
    0xFFFFFF89u, 0xFFFFFF10u, 0xFFFFFFFFu, 0u, 1u,
    60u, 300u, 4095u, 4096u, 0x80000005u
};

Particle spawn(uint slot, uint generation) {
    uint place = slot % (ProbeSide * ProbeSide);
    float2 cell = float2(float(place % ProbeSide), float(place / ProbeSide));
    float2 xz = (cell / float(ProbeSide - 1u) * 2.0 - 1.0) * effect.emitter.w;
    uint step = ProbeSteps[(slot / (ProbeSide * ProbeSide)) % 10u];
    float2 wind = windAt(xz, step);
    float sea = seaHeight(xz, step, SEA_VERTEX_WAVES, true);

    Particle p = (Particle)0;
    p.position = float3(xz.x, 0.0, xz.y);
    p.velocity = float3(wind.x, 0.0, wind.y);
    p.lifeSteps = EventLife;
    p.generation = generation;
    // the step, bit for bit, and the sea there
    p.custom = float4(asfloat(step), sea, 0.0, 0.0);
    return p;
}

void update(inout Particle p, uint slot) {}

#include "ParticleKernel.slang"
