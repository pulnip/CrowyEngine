// A meteor shower: streaks crossing the sky in front of the island, bright
// at the head, flaring in and out, then resting a while before the next.

#include "Effect.slang"

// what the desc's emitter and params mean here
float3 domeCenter() { return effect.emitter.xyz; }
float domeRadius() { return effect.emitter.w; }
float speed() { return effect.params[0].x; }
float streakSeconds() { return effect.params[0].y; }
float brightness() { return effect.params[0].z; }

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    // ahead of the camera, a little over the horizon
    float azimuth = 1.5707963 +
                    (particleRandom(slot, generation, 1u) - 0.5) * 1.6;
    float elevation = lerp(0.12, 0.42, particleRandom(slot, generation, 2u));
    float3 toSky = float3(
        cos(azimuth) * cos(elevation),
        sin(elevation),
        sin(azimuth) * cos(elevation)
    );
    p.position = domeCenter() + toSky * domeRadius();

    // across the sky and a little down
    float heading = (particleRandom(slot, generation, 3u) - 0.5) * 2.4;
    float3 across = float3(cos(heading), -0.3, sin(heading) * 0.2);
    p.velocity = normalize(across) * speed();

    uint burning = 18u + effectHash(effect.seed, slot, generation, 4u) % 24u;
    uint resting = 60u + effectHash(effect.seed, slot, generation, 5u) % 180u;
    p.lifeSteps = burning + resting;
    // the steps it burns before it rests
    p.custom = float4(float(burning), 0.0, 0.0, 0.0);
    p.generation = generation;
    p.size = 0.1;
    return p;
}

void update(inout Particle p, uint slot) {
    p.position += p.velocity * effect.dt;
}

struct MeteorVertex {
    float4 position : SV_Position;
    float2 corner : TEXCOORD0;
    float glow : TEXCOORD1;
};

[shader("vertex")]
MeteorVertex vs_meteors(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float burning = p.custom.x;
    float t = float(p.ageSteps) / max(burning, 1.0);
    bool showing = t < 1.0;
    float2 corner = stripCorner(vertexID);
    float3 axis = normalize(p.velocity);
    float3 toCamera = normalize(cameraPosition.xyz - p.position);
    float3 side = normalize(cross(axis, toCamera));
    float streak = length(p.velocity) * streakSeconds();
    float width = showing ? p.size : 0.0;
    float3 world = p.position + side * (corner.x * width) -
                   axis * ((corner.y * 0.5 + 0.5) * streak);

    MeteorVertex o;
    o.position = toClip(world);
    o.corner = corner;
    o.glow = showing ? sin(3.1415927 * t) : 0.0;
    return o;
}

[shader("fragment")]
float4 fs_meteors(MeteorVertex v) : SV_Target {
    float across = 1.0 - abs(v.corner.x);
    float along = 0.5 - 0.5 * v.corner.y;
    float3 color = float3(0.85, 0.9, 1.0) * brightness();
    return float4(color * across * along * along * v.glow, 1.0);
}

#include "ParticleKernel.slang"
