// Stars on a dome that follows the camera, each its own brightness and
// color, twinkling more toward the horizon.

#include "Effect.slang"
#include "IslandScene.h"

// what the desc's emitter and params mean here
float domeRadius() { return effect.emitter.w; }
float lowestSine() { return effect.params[0].x; }
float halfPixels() { return effect.params[0].y; }
uint twinkleSteps() { return uint(effect.params[0].z); }
float faintest() { return effect.params[1].x; }
float brightSpan() { return effect.params[1].y; }
float brightPower() { return effect.params[1].z; }

float3 starColor(float pick) {
    if(pick < 0.6)
        return float3(1.0, 1.0, 1.0);
    if(pick < 0.8)
        return float3(0.8, 0.88, 1.0);
    if(pick < 0.95)
        return float3(1.0, 0.9, 0.72);
    return float3(1.0, 0.72, 0.5);
}

// born once, at an area-uniform direction over the dome
Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    float azimuth = 6.2831853 * particleRandom(slot, generation, 1u);
    float sine = lerp(lowestSine(), 1.0, particleRandom(slot, generation, 2u));
    float cosine = sqrt(1.0 - sine * sine);
    p.position = float3(sin(azimuth) * cosine, sine, cos(azimuth) * cosine);
    p.lifeSteps = EventLife;
    p.generation = generation;
    p.size = faintest() +
             brightSpan() *
                 pow(particleRandom(slot, generation, 3u), brightPower());
    p.custom = float4(starColor(particleRandom(slot, generation, 4u)), 0.0);
    return p;
}

void update(inout Particle p, uint slot) {}

// smooth noise in time: a fresh draw every twinkleSteps, eased between
float twinkle(uint slot, uint step) {
    uint key = step / twinkleSteps();
    float t = float(step % twinkleSteps()) / float(twinkleSteps());
    float from = effectRandom(effect.seed, slot, key, 9u);
    float to = effectRandom(effect.seed, slot, key + 1u, 9u);
    return lerp(from, to, smoothstep(0.0, 1.0, t));
}

struct StarVertex {
    float4 position : SV_Position;
    float2 corner : TEXCOORD0;
    float3 color : TEXCOORD1;
};

[shader("vertex")]
StarVertex vs_stars(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float3 dir = p.position;
    float3 center = cameraPosition.xyz + dir * domeRadius();
    float halfSize = halfPixels() * ISLAND_PIXEL_ANGLE * domeRadius();
    float2 corner = stripCorner(vertexID);

    // the thicker air near the horizon dims and stirs a star
    float amplitude = lerp(0.35, 0.08, dir.y);
    float stir =
        1.0 + amplitude * (2.0 * twinkle(slot, effect.worldStep) - 1.0);
    float extinction = lerp(0.35, 1.0, smoothstep(0.0349, 0.2588, dir.y));

    StarVertex o;
    o.position = toClip(billboardCorner(center, halfSize, corner));
    o.corner = corner;
    o.color = p.custom.rgb * (p.size * stir * extinction);
    return o;
}

[shader("fragment")]
float4 fs_stars(StarVertex v) : SV_Target {
    return float4(v.color * exp(-3.0 * dot(v.corner, v.corner)), 1.0);
}

#include "ParticleKernel.slang"
