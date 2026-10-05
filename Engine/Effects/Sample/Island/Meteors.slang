// A meteor shower: every meteor runs straight away from one radiant off the
// upper left, flashing where it ignites, may pop; some fly in volleys.

#include "Effect.slang"
#include "IslandScene.h"

// the last four slots fly as one volley behind the first of them
static const uint VolleySlots = 4u;
// the steps a meteor waits inside its flash, and the flash's whole life
static const float LaunchStep = 6.0;
static const float FlashSteps = 18.0;
static const float PopChance = 0.5;
// a trail's hue at the head and at the tail: ice, green, gold
static const float3 HeadTints[3] = {
    float3(0.70, 0.90, 1.00),
    float3(0.65, 1.00, 0.85),
    float3(1.00, 0.85, 0.60)
};
static const float3 TailTints[3] = {
    float3(0.50, 0.40, 1.00),
    float3(0.25, 0.90, 0.45),
    float3(1.00, 0.45, 0.25)
};
static const float3 WhiteHot = float3(1.0, 0.97, 0.92);

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
// the sine of the elevation no meteor ends below
float lowestSine() { return effect.emitter.w; }
// where meteors ignite: x and y over z in front of the island
float4 startWindow() { return effect.params[0]; }
// radians a meteor sweeps, and its mean speed in radians a second
float2 sweeps() { return effect.params[1].xy; }
float2 speeds() { return effect.params[1].zw; }
float headPeak() { return effect.params[2].x; }
float flashOverHead() { return effect.params[2].y; }
// the steps the trail's tail runs behind the head
float trailLag() { return effect.params[2].z; }
float fireballChance() { return effect.params[2].w; }

float3 awayFromRadiant(float3 at) {
    return normalize(at * dot(at, radiant()) - radiant());
}

Particle spawn(uint slot, uint generation) {
    uint leader = effect.count - VolleySlots;
    uint member = slot > leader ? slot - leader : 0u;
    uint owner = member > 0u ? leader : slot;
    // eight cells over the sky, each slot's meteors taking them in turn
    uint cell = (owner * 3u + generation * 5u) % 8u;
    float2 inCell = float2(
        particleRandom(owner, generation, 1u),
        particleRandom(owner, generation, 2u)
    );
    float2 grid = float2(float(cell % 4u), float(cell / 4u));
    float2 place = (grid + inCell) / float2(4.0, 2.0);
    float4 window = startWindow();
    float3 start = normalize(float3(
        lerp(window.x, window.y, place.x),
        lerp(window.z, window.w, place.y),
        1.0
    ));
    float3 heading = awayFromRadiant(start);
    // a volley's members ignite beside their leader, as near parallel
    if(member > 0u) {
        float side = (member & 1u) != 0u ? 1.0 : -1.0;
        float along = lerp(-40.0, 80.0, particleRandom(slot, generation, 3u));
        float across =
            side * lerp(60.0, 200.0, particleRandom(slot, generation, 4u));
        float3 shift = heading * along + cross(start, heading) * across;
        start = normalize(start + shift * ISLAND_PIXEL_ANGLE);
        heading = awayFromRadiant(start);
    }

    // near the radiant a meteor runs shorter and slower
    float spread = 0.6 + 0.4 * length(cross(start, radiant()));
    float fireballs = slot == leader ? 0.3 : fireballChance();
    bool fireball =
        member == 0u && particleRandom(slot, generation, 7u) < fireballs;
    float scale = fireball ? 1.6 : 1.0;
    float sweepPick = particleRandom(slot, generation, 8u);
    float sweep = lerp(sweeps().x, sweeps().y, sweepPick) * spread *
                  (fireball ? 1.3 : 1.0);
    // no further than where its circle comes down to the lowest elevation
    float lift = length(float2(start.y, heading.y));
    float reach = atan2(heading.y, start.y) +
                  acos(saturate(lowestSine() / lift));
    sweep = clamp(reach, 0.05, sweep);
    float speedPick = particleRandom(slot, generation, 9u);
    float speed = lerp(speeds().x, speeds().y, speedPick) * spread *
                  (fireball ? 0.8 : 1.0);
    float travel = clamp(round(sweep / (speed * effect.dt)), 18.0, 90.0);
    float hue = particleRandom(slot, generation, 10u);
    uint palette = fireball ? 1u : (hue < 0.65 ? 0u : (hue < 0.85 ? 1u : 2u));
    bool pops = fireball || particleRandom(slot, generation, 11u) < PopChance;
    uint volley = 2u + effectHash(effect.seed, leader, generation, 12u) % 3u;
    uint resting = (slot == leader ? 600u : 540u) +
                   effectHash(effect.seed, slot, generation, 13u) % 420u;

    Particle p = (Particle)0;
    p.position = start;
    p.velocity = heading;
    // its sweep, its travel's steps, its scale, its palette and whether it pops
    p.custom = float4(sweep, travel, scale, float(palette + (pops ? 4u : 0u)));
    // its brightness: none for a member the volley leaves out
    p.size = member < volley ? 1.0 - 0.15 * float(member) : 0.0;
    p.lifeSteps = member > 0u
        ? EventLife
        : uint(LaunchStep + travel + trailLag() * scale) + resting;
    p.generation = generation;
    return p;
}

// it moves as a function of its age, not by steps
void update(inout Particle p, uint slot) {}

// fast off the mark, slowing as it burns out
float eased(float u) { return u * (1.6 - 0.6 * u); }

float angleAt(Particle p, float age) {
    return p.custom.x * eased(saturate((age - LaunchStep) / p.custom.y));
}

float3 pointAt(Particle p, float angle) {
    return p.position * cos(angle) + p.velocity * sin(angle);
}

float3 headingAt(Particle p, float angle) {
    return p.velocity * cos(angle) - p.position * sin(angle);
}

float overSea(float3 dir) { return smoothstep(0.0349, 0.0785, dir.y); }

// a volley's member follows its leader's generation, a gap behind per place
Particle meteor(uint slot, out float age) {
    Particle p = loadParticle(slot);
    age = float(p.ageSteps);
    uint leader = effect.count - VolleySlots;
    if(slot > leader) {
        Particle lead = loadParticle(leader);
        uint gap =
            8u + effectHash(effect.seed, leader, lead.generation, 14u) % 9u;
        p = spawn(slot, lead.generation);
        age = float(lead.ageSteps) - float((slot - leader) * gap);
    }
    return p;
}

// `pixels` across and along `heading` from `dir`, on the shell
float3 shellCorner(float3 dir, float3 heading, float2 pixels) {
    float3 offset = cross(dir, heading) * pixels.x + heading * pixels.y;
    return cameraPosition.xyz +
           (dir + offset * ISLAND_PIXEL_ANGLE) * shellRadius();
}

struct TrailVertex {
    float4 position : SV_Position;
    // pixels across, and from the tail (0) to the head (1)
    float2 place : TEXCOORD0;
    float glow : TEXCOORD1;
    nointerpolation float palette : TEXCOORD2;
    // the trail's length in pixels
    nointerpolation float span : TEXCOORD3;
};

[shader("vertex")]
TrailVertex vs_trails(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    float age;
    Particle p = meteor(slot, age);
    float scale = p.custom.z;
    float headAngle = angleAt(p, age);
    float tailAngle = angleAt(p, age - trailLag() * scale);
    float2 corner = stripCorner(vertexID);
    float along = corner.y * 0.5 + 0.5;
    float angle = lerp(tailAngle, headAngle, along);
    float3 dir = pointAt(p, angle);
    bool showing = age >= 0.0 && headAngle > tailAngle;
    float halfPixels = showing ? 6.0 * scale : 0.0;
    // the head's light lingers on the trail as it draws in
    float dying = exp(-max(age - LaunchStep - p.custom.y, 0.0) / 5.0);
    float2 across = float2(corner.x * halfPixels, 0.0);

    TrailVertex o;
    o.position = toClip(shellCorner(dir, headingAt(p, angle), across));
    o.place = float2(corner.x * 6.0, along);
    o.glow = 0.5 * headPeak() * scale * p.size * dying * overSea(dir);
    o.palette = p.custom.w;
    o.span = (headAngle - tailAngle) / ISLAND_PIXEL_ANGLE;
    return o;
}

// a white core in a tinted halo, bright just behind the head, long and faint
[shader("fragment")]
float4 fs_trails(TrailVertex v) : SV_Target {
    float a = v.place.y;
    float d = v.place.x;
    float coreWidth = lerp(0.4, 1.1, a);
    float haloWidth = lerp(1.5, 4.0, a);
    float core = exp(-d * d / (coreWidth * coreWidth));
    float halo = 0.25 * exp(-d * d / (haloWidth * haloWidth)) *
                 (1.0 - d * d / 36.0);
    float wake = 0.55 * exp(-9.0 * (1.0 - a)) + 0.45 * exp(-2.2 * (1.0 - a));
    // the last pixels round off, so a trail drawing in has no square end
    wake *= saturate((1.0 - a) * v.span / 3.0);
    uint palette = uint(v.palette) & 3u;
    float3 tint = lerp(TailTints[palette], HeadTints[palette], a * a);
    float3 color = core * lerp(tint, float3(1.0, 1.0, 1.0), 0.5 * a) +
                   halo * tint;
    return float4(color * (wake * v.glow), 1.0);
}

struct GlintVertex {
    float4 position : SV_Position;
    // pixels from the center, x along the motion
    float2 place : TEXCOORD0;
    // core radius, ray length, quad half size, the core's stretch along x
    nointerpolation float4 shape : TEXCOORD1;
    // the glint's light, then a pop's ray length and light
    nointerpolation float3 light : TEXCOORD2;
    nointerpolation float3 tint : TEXCOORD3;
};

// a four-point star as (white core, tinted halo and rays), its x ray longer
float2 glint(float2 q, float4 shape) {
    float r2 = dot(q, q);
    float2 c = float2(q.x / shape.w, q.y) / shape.x;
    float window = saturate(1.0 - sqrt(r2) / shape.z);
    float2 tip = saturate(1.0 - abs(q) / float2(shape.y, 0.7 * shape.y));
    float rays = exp(-q.y * q.y / 0.5) * tip.x * tip.x +
                 exp(-q.x * q.x / 0.5) * tip.y * tip.y;
    float halo = pow(1.0 + r2 / (6.25 * shape.x * shape.x), -1.5);
    return float2(exp(-dot(c, c)), (0.15 * halo + rays) * window * window);
}

float4 glintColor(GlintVertex v) {
    float2 light = glint(v.place, v.shape) * v.light.x;
    // a pop's rays sit between the head's
    float2 turned =
        float2(v.place.x + v.place.y, v.place.y - v.place.x) * 0.7071;
    float4 popShape =
        float4(1.25 * v.shape.x, max(v.light.y, 1.0), v.shape.z, 1.0);
    light += glint(turned, popShape) * v.light.z;
    return float4(WhiteHot * light.x + v.tint * light.y, 1.0);
}

// the head: still inside its flash, then smeared along its run
[shader("vertex")]
GlintVertex vs_heads(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    float age;
    Particle p = meteor(slot, age);
    float scale = p.custom.z;
    float u = (age - LaunchStep) / p.custom.y;
    float angle = angleAt(p, age);
    float3 dir = pointAt(p, angle);
    bool pops = (uint(p.custom.w) & 4u) != 0u;
    float pop = pops ? 1.8 * exp(-(u - 0.86) * (u - 0.86) / 0.001225) : 0.0;
    float fade = 1.0 - smoothstep(pops ? 0.9 : 0.75, 1.0, u);
    float rise = saturate((age + 1.0) / 3.0);
    float stepPixels = u > 0.0
        ? p.custom.x * (1.6 - 1.2 * saturate(u)) /
              (p.custom.y * ISLAND_PIXEL_ANGLE)
        : 0.0;
    float twinkle = 1.0 + 0.15 * sin(0.94 * age + 2.4 * float(slot));
    float halfPixels = age >= 0.0 && u < 1.0 ? 16.0 * scale : 0.0;
    float light = headPeak() * scale * p.size * overSea(dir);
    float2 corner = stripCorner(vertexID);

    GlintVertex o;
    o.position = toClip(
        shellCorner(dir, headingAt(p, angle), corner * halfPixels)
    );
    o.place = corner.yx * halfPixels;
    o.shape = float4(
        1.2 * scale,
        7.0 * scale * twinkle,
        halfPixels,
        1.0 + stepPixels / 6.0
    );
    o.light = float3(
        light * rise * fade,
        12.0 * scale * saturate(pop),
        light * pop
    );
    o.tint = HeadTints[uint(p.custom.w) & 3u];
    return o;
}

[shader("fragment")]
float4 fs_heads(GlintVertex v) : SV_Target { return glintColor(v); }

// the flash, where it ignites: up in three steps, then dying in place as the
// head shoots off; its long ray points the way it will go
[shader("vertex")]
GlintVertex vs_flashes(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    float age;
    Particle p = meteor(slot, age);
    float scale = p.custom.z;
    float burst = age < 2.0
        ? (age + 1.0) * (age + 1.0) / 9.0
        : exp(-(age - 2.0) / 4.0) * saturate((FlashSteps - age) / 4.0);
    float reach = age < 2.0
        ? 0.55 + 0.35 * age
        : 1.1 * (1.0 - 0.45 * saturate((age - 2.0) / 14.0));
    float halfPixels = age >= 0.0 && age < FlashSteps ? 26.0 * scale : 0.0;
    float light = headPeak() * flashOverHead() * scale * p.size * burst;
    float2 corner = stripCorner(vertexID);

    GlintVertex o;
    o.position =
        toClip(shellCorner(p.position, p.velocity, corner * halfPixels));
    o.place = corner.yx * halfPixels;
    o.shape = float4(1.6 * scale, 22.0 * scale * reach, halfPixels, 1.0);
    o.light = float3(light * overSea(p.position), 0.0, 0.0);
    o.tint = lerp(
        HeadTints[uint(p.custom.w) & 3u],
        float3(1.0, 1.0, 1.0),
        0.3
    );
    return o;
}

[shader("fragment")]
float4 fs_flashes(GlintVertex v) : SV_Target { return glintColor(v); }

#include "ParticleKernel.slang"
