// Flames inside the log tripod: short tongues rising off the coals, drawn
// in toward the logs' crossing, white-yellow to orange to a dull red.

#include "Effect.slang"
#include "Weather.slang"

// what the desc's emitter and params mean here
float3 bedCenter() { return effect.emitter.xyz; }
float bedRadius() { return effect.emitter.w; }
float riseMin() { return effect.params[0].x; }
float riseMax() { return effect.params[0].y; }
float lift() { return effect.params[0].z; }
float drag() { return effect.params[0].w; }
// how far above the coals the logs cross
float crossing() { return effect.params[1].x; }
float pull() { return effect.params[1].y; }
float2 halfSizes() { return effect.params[1].zw; }
float peak() { return effect.params[2].x; }
float stretch() { return effect.params[2].y; }
uint lifeMin() { return uint(effect.params[2].z); }
uint lifeSpan() { return uint(effect.params[2].w); }

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    float angle = 6.2831853 * particleRandom(slot, generation, 1u);
    float radius = bedRadius() * sqrt(particleRandom(slot, generation, 2u));
    p.position = bedCenter() +
                 float3(sin(angle) * radius, 0.0, cos(angle) * radius);
    float rise =
        lerp(riseMin(), riseMax(), particleRandom(slot, generation, 3u));
    p.velocity = float3(
        (particleRandom(slot, generation, 4u) - 0.5) * 0.2,
        rise,
        (particleRandom(slot, generation, 5u) - 0.5) * 0.2
    );
    p.lifeSteps =
        lifeMin() + effectHash(effect.seed, slot, generation, 6u) % lifeSpan();
    p.generation = generation;
    p.size = lerp(
        halfSizes().x,
        halfSizes().y,
        particleRandom(slot, generation, 7u)
    );
    // the flicker's phase and rate
    p.custom = float4(
        6.2831853 * particleRandom(slot, generation, 8u),
        lerp(18.0, 30.0, particleRandom(slot, generation, 9u)),
        0.0,
        0.0
    );
    return p;
}

void update(inout Particle p, uint slot) {
    float dt = effect.dt;
    float height = p.position.y - bedCenter().y;
    float2 offset = p.position.xz - bedCenter().xz;
    float2 inward = -offset * pull() * saturate(height / crossing());
    // the tipi shelters the fire, so the wind only leans it a little
    float2 wind = windAt(p.position.xz, effect.worldStep) * 0.12;
    float3 air = float3(wind.x, 0.0, wind.y);
    float3 accel =
        float3(inward.x, lift(), inward.y) + (air - p.velocity) * drag();
    p.velocity += accel * dt;
    p.position += p.velocity * dt;
}

struct FlameVertex {
    float4 position : SV_Position;
    float2 corner : TEXCOORD0;
    float3 color : TEXCOORD1;
};

// a camera-facing tongue stretched along its rise, shrinking as it burns
[shader("vertex")]
FlameVertex vs_flames(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float t = float(p.ageSteps) / float(max(p.lifeSteps, 1u));
    float size = p.size * (1.0 - 0.75 * t);
    float2 corner = stripCorner(vertexID);
    float3 axis = normalize(p.velocity + float3(0.0, 1e-3, 0.0));
    float3 side = normalize(cross(axis, cameraPosition.xyz - p.position));
    float3 world = p.position + side * (corner.x * size) +
                   axis * (corner.y * size * stretch());

    float3 hot = float3(1.0, 0.72, 0.32);
    float3 warm = float3(1.0, 0.4, 0.07);
    float3 cold = float3(0.6, 0.1, 0.02);
    float3 color = t < 0.3 ? lerp(hot, warm, t / 0.3)
                           : lerp(warm, cold, saturate((t - 0.3) / 0.5));
    float age = float(p.ageSteps) * effect.dt;
    float flicker = 0.85 + 0.15 * sin(p.custom.y * age + p.custom.x);
    float strength = peak() * pow(1.0 - t, 1.5) * flicker *
                     fireBreath(effect.worldStep);

    FlameVertex o;
    o.position = toClip(world);
    o.corner = corner;
    o.color = color * strength;
    return o;
}

[shader("fragment")]
float4 fs_flames(FlameVertex v) : SV_Target {
    float falloff = saturate(1.0 - dot(v.corner, v.corner));
    return float4(v.color * falloff * falloff, 1.0);
}

#include "ParticleKernel.slang"
