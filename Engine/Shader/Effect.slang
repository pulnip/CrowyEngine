#pragma once

// What a particle effect's file includes first: the particle, the one push
// its kernel and its draws share (Metal allows one push struct per module),
// and the helpers its draws use. Mirrored by ParticleEffects.hpp.

#include "Random.slang"
#include "View.slang"

// 64 bytes; a slot is always the same particle, reborn with generation + 1
struct Particle {
    float3 position;
    uint ageSteps;
    float3 velocity;
    uint lifeSteps;
    // the effect's own four lanes
    float4 custom;
    uint generation;
    float size;
    uint _pad0;
    uint _pad1;
};

struct EffectPush {
    DescriptorHandle<RWStructuredBuffer<Particle>> particlesRW;
    DescriptorHandle<StructuredBuffer<Particle>> particles;
    uint count;
    // steps since the effect began; time is step * dt, never the wall clock
    uint step;
    float dt;
    uint seed;
    float4 cameraRight;
    float4 cameraUp;
    float4 emitter;
    float4 params[3];
};

[[vk::push_constant]]
ConstantBuffer<EffectPush> effect : register(b0);

// the draw the first step staggers ages with; an effect's own draws keep
// to small k
static const uint ParticleStaggerDraw = 0xA6Eu;

// draw k of this particle's life, uniform in [0, 1)
float particleRandom(uint slot, uint generation, uint k) {
    return effectRandom(effect.seed, slot, generation, k);
}

Particle loadParticle(uint slot) {
    StructuredBuffer<Particle> particles = effect.particles;
    return particles[slot];
}

// a four-vertex strip's corner: (-1, -1), (1, -1), (-1, 1), (1, 1)
float2 stripCorner(uint vertexID) {
    return float2(
        (vertexID & 1u) != 0u ? 1.0 : -1.0,
        (vertexID & 2u) != 0u ? 1.0 : -1.0
    );
}

// a camera-facing quad's corner around `center`, `halfSize` metres out
float3 billboardCorner(float3 center, float halfSize, float2 corner) {
    float3 offset = effect.cameraRight.xyz * corner.x +
                    effect.cameraUp.xyz * corner.y;
    return center + offset * halfSize;
}

float4 toClip(float3 world) {
    return mul(viewProj, float4(world, 1.0));
}
