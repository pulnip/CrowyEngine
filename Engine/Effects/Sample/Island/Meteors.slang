// A meteor shower: streaks leaving a radiant low on the left along great
// circles across the sky ahead, burning a moment, resting a while.

#include "Effect.slang"
#include "IslandScene.h"

// a direction from its azimuth (from +z toward +x) and elevation
float3 skyDirection(float azimuth, float elevation) {
    return float3(
        sin(azimuth) * cos(elevation),
        sin(elevation),
        cos(azimuth) * cos(elevation)
    );
}

// what the desc's emitter and params mean here
float3 radiant() { return skyDirection(effect.emitter.x, effect.emitter.y); }
float shellRadius() { return effect.emitter.z; }
float2 startAzimuths() { return effect.params[0].xy; }
float2 startElevations() { return effect.params[0].zw; }
// radians a second along the circle
float2 speeds() { return effect.params[1].xy; }
float trailSeconds() { return effect.params[1].z; }
float peak() { return effect.params[1].w; }
float halfPixels() { return effect.params[2].x; }
float fireballChance() { return effect.params[2].y; }

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    float azimuth = lerp(
        startAzimuths().x,
        startAzimuths().y,
        particleRandom(slot, generation, 1u)
    );
    float elevation = lerp(
        startElevations().x,
        startElevations().y,
        particleRandom(slot, generation, 2u)
    );
    float3 start = skyDirection(azimuth, elevation);
    float3 from = radiant();
    // where it starts, and the way along its circle away from the radiant
    p.position = start;
    p.velocity = normalize(start * dot(start, from) - from);

    uint burning = 21u + effectHash(effect.seed, slot, generation, 3u) % 28u;
    uint resting = 240u + effectHash(effect.seed, slot, generation, 4u) % 360u;
    p.lifeSteps = burning + resting;
    p.generation = generation;
    float speed =
        lerp(speeds().x, speeds().y, particleRandom(slot, generation, 5u));
    bool fireball = particleRandom(slot, generation, 6u) < fireballChance();
    // the steps it burns, its speed, and how bright it burns
    p.custom = float4(float(burning), speed, fireball ? 2.5 : 1.0, 0.0);
    return p;
}

// it moves along its circle as a function of its age, not by steps
void update(inout Particle p, uint slot) {}

float3 headAt(Particle p, float seconds) {
    float angle = p.custom.y * max(seconds, 0.0);
    return p.position * cos(angle) + p.velocity * sin(angle);
}

struct MeteorVertex {
    float4 position : SV_Position;
    // across the streak, and from the tail (0) to the head (1)
    float2 place : TEXCOORD0;
    float glow : TEXCOORD1;
};

[shader("vertex")]
MeteorVertex vs_meteors(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float t = float(p.ageSteps) / max(p.custom.x, 1.0);
    bool showing = t < 1.0;
    float seconds = float(p.ageSteps) * effect.dt;
    float3 head = headAt(p, seconds);
    float3 tail = headAt(p, seconds - trailSeconds());

    float2 corner = stripCorner(vertexID);
    float along = corner.y * 0.5 + 0.5;
    float3 dir = normalize(lerp(tail, head, along));
    float angle = p.custom.y * seconds;
    float3 heading = p.velocity * cos(angle) - p.position * sin(angle);
    float3 side = normalize(cross(heading, dir));
    float halfWidth =
        showing ? halfPixels() * ISLAND_PIXEL_ANGLE * shellRadius() : 0.0;
    float3 world = cameraPosition.xyz + dir * shellRadius() +
                   side * (corner.x * halfWidth);

    float envelope = smoothstep(0.0, 0.15, t) * (1.0 - smoothstep(0.7, 1.0, t));
    float overHorizon = smoothstep(0.0698, 0.1392, head.y);

    MeteorVertex o;
    o.position = toClip(world);
    o.place = float2(corner.x, along);
    o.glow = showing ? envelope * overHorizon * peak() * p.custom.z : 0.0;
    return o;
}

[shader("fragment")]
float4 fs_meteors(MeteorVertex v) : SV_Target {
    float across = exp(-2.9 * v.place.x * v.place.x);
    float toTail = exp(-3.0 * (1.0 - v.place.y));
    float3 color = lerp(
        float3(0.7, 1.0, 0.8),
        float3(1.0, 0.93, 0.8),
        v.place.y * v.place.y
    );
    return float4(color * (across * toTail * v.glow), 1.0);
}

#include "ParticleKernel.slang"
