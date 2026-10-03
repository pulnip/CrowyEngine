// Campfire embers: born on a disc over the fire, carried up by the heat,
// swirling, cooling from yellow-white through orange to a dull red.

#include "Effect.slang"

// what the desc's emitter and params mean here
float3 discCenter() { return effect.emitter.xyz; }
float discRadius() { return effect.emitter.w; }
float riseMin() { return effect.params[0].x; }
float riseMax() { return effect.params[0].y; }
float buoyancy() { return effect.params[0].z; }
float drag() { return effect.params[0].w; }
float swirl() { return effect.params[1].x; }
float halfSize() { return effect.params[1].y; }
float brightness() { return effect.params[1].z; }

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    float angle = 6.2831853 * particleRandom(slot, generation, 1u);
    float radius = discRadius() * sqrt(particleRandom(slot, generation, 2u));
    p.position = discCenter() +
                 float3(cos(angle) * radius, 0.0, sin(angle) * radius);

    float rise =
        lerp(riseMin(), riseMax(), particleRandom(slot, generation, 3u));
    p.velocity = float3(
        (particleRandom(slot, generation, 4u) - 0.5) * 0.5,
        rise,
        (particleRandom(slot, generation, 5u) - 0.5) * 0.5
    );
    p.lifeSteps = 70u + effectHash(effect.seed, slot, generation, 6u) % 110u;
    p.generation = generation;
    p.size = halfSize() * lerp(0.6, 1.4, particleRandom(slot, generation, 7u));
    // the swirl's phase
    float phase = 6.2831853 * particleRandom(slot, generation, 8u);
    p.custom = float4(phase, 0.0, 0.0, 0.0);
    return p;
}

void update(inout Particle p, uint slot) {
    float dt = effect.dt;
    float age = float(p.ageSteps) * dt;
    float phase = p.custom.x;
    float3 sway = float3(
        sin(age * 3.1 + phase),
        0.0,
        cos(age * 2.3 + phase)
    ) * swirl();
    float3 accel = float3(0.0, buoyancy(), 0.0) + sway - p.velocity * drag();
    p.velocity += accel * dt;
    p.position += p.velocity * dt;
}

struct EmberVertex {
    float4 position : SV_Position;
    float2 corner : TEXCOORD0;
    // 1 at birth, 0 at death
    float heat : TEXCOORD1;
};

[shader("vertex")]
EmberVertex vs_embers(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float life = float(p.ageSteps) / float(max(p.lifeSteps, 1u));
    float2 corner = stripCorner(vertexID);

    EmberVertex o;
    o.position = toClip(billboardCorner(p.position, p.size, corner));
    o.corner = corner;
    o.heat = 1.0 - life;
    return o;
}

[shader("fragment")]
float4 fs_embers(EmberVertex v) : SV_Target {
    float r2 = dot(v.corner, v.corner);
    float falloff = saturate(1.0 - r2);
    falloff *= falloff;

    float3 hot = float3(1.0, 0.85, 0.45);
    float3 warm = float3(1.0, 0.38, 0.06);
    float3 cold = float3(0.5, 0.05, 0.01);
    float3 color = v.heat > 0.6
        ? lerp(warm, hot, (v.heat - 0.6) / 0.4)
        : lerp(cold, warm, v.heat / 0.6);
    float fade = saturate(v.heat * 3.0);

    return float4(color * brightness() * falloff * fade, 1.0);
}

#include "ParticleKernel.slang"
