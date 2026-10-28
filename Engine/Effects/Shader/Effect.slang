#pragma once

// What an effect's file includes first: the particle, the push its kernel
// and draws share (one push a module, for Metal), and the draws' helpers.

#include "EffectConstants.h"
#include "Random.slang"
#include "View.slang"
#include "WorldClock.slang"

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
    // where the effect sits, which is all that moves with it; w is unused
    float4 origin;
    float4 params[4];
    // the world's step this dispatch or draw shows, shared with the scene;
    // a prewarm counts back from it
    uint worldStep;
    // how far the effect reaches around its origin
    float size;
    uint _pad0;
    uint _pad1;
};

[[vk::push_constant]]
ConstantBuffer<EffectPush> effect : register(b0);

static const uint ParticleStaggerDraw = CROWY_EFFECT_STAGGER_DRAW;
// a life no age reaches: the effect's update rebirths the particle on an
// event, and the first step leaves its age at 0
static const uint EventLife = 0xFFFFFFFFu;

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

// a camera-facing quad's corner around `center`, `halfSize` meters out
float3 billboardCorner(float3 center, float halfSize, float2 corner) {
    float3 offset = effect.cameraRight.xyz * corner.x +
                    effect.cameraUp.xyz * corner.y;
    return center + offset * halfSize;
}

float4 toClip(float3 world) {
    return mul(viewProj, float4(world, 1.0));
}
