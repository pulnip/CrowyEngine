// Rain over the island: drops fall through a box, and where one meets the
// sea or the sand it leaves a ring that spreads and fades.
//
//   emitter    xyz the box's center, w its half height
//   params[0]  x, y the box's half width and depth; z fall speed; w wind x
//   params[1]  x wind z; y streak length in seconds; z streak brightness;
//              w ring opacity
//   params[2]  the island: xyz an ellipsoid's radii, w its center's height

#include "Effect.slang"

// how long a ring spreads
static const uint RingSteps = 36u;
// custom.w before a drop's first landing
static const float NoLanding = -1.0e6;

// the sea at 0, or the island's crown where it rises above it
float surfaceHeight(float2 xz) {
    float3 radii = effect.params[2].xyz;
    float2 q = xz / radii.xz;
    float inside = 1.0 - dot(q, q);
    float crown = inside > 0.0
        ? effect.params[2].w + radii.y * sqrt(inside)
        : NoLanding;
    return max(0.0, crown);
}

// a drop in the box: at any height when first born, at the top after
float3 dropPosition(uint slot, uint generation, bool anywhere) {
    float halfHeight = effect.emitter.w;
    float x = (particleRandom(slot, generation, 1u) * 2.0 - 1.0) *
              effect.params[0].x;
    float z = (particleRandom(slot, generation, 2u) * 2.0 - 1.0) *
              effect.params[0].y;
    float y = anywhere
        ? (particleRandom(slot, generation, 3u) * 2.0 - 1.0) * halfHeight
        : halfHeight;
    return effect.emitter.xyz + float3(x, y, z);
}

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    p.position = dropPosition(slot, generation, true);
    p.velocity = float3(
        effect.params[0].w,
        -effect.params[0].z,
        effect.params[1].x
    );
    // a drop is reborn where it lands, never by age
    p.lifeSteps = 0xFFFFFFFFu;
    p.generation = generation;
    p.size = 0.006;
    p.custom = float4(0.0, 0.0, 0.0, NoLanding);
    return p;
}

void update(inout Particle p, uint slot) {
    float3 next = p.position + p.velocity * effect.dt;
    float ground = surfaceHeight(next.xz);
    if(next.y > ground) {
        p.position = next;
        return;
    }

    // the landing and its step, then the drop again at the box's top
    p.custom = float4(next.x, ground, next.z, float(effect.step));
    p.generation += 1u;
    p.position = dropPosition(slot, p.generation, false);
    p.ageSteps = 0u;
}

struct RainVertex {
    float4 position : SV_Position;
    float2 corner : TEXCOORD0;
    // 1 while it shows, 0 when it is gone
    float fade : TEXCOORD1;
};

// a thin quad from the drop back along its fall, turned to the camera
[shader("vertex")]
RainVertex vs_streaks(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float2 corner = stripCorner(vertexID);
    float3 axis = normalize(p.velocity);
    float3 toCamera = normalize(cameraPosition.xyz - p.position);
    float3 side = normalize(cross(axis, toCamera));
    float streak = length(p.velocity) * effect.params[1].y;
    float3 world = p.position + side * (corner.x * p.size) -
                   axis * ((corner.y * 0.5 + 0.5) * streak);

    RainVertex o;
    o.position = toClip(world);
    o.corner = corner;
    o.fade = 1.0;
    return o;
}

[shader("fragment")]
float4 fs_streaks(RainVertex v) : SV_Target {
    float across = 1.0 - abs(v.corner.x);
    // bright at the drop, faint at the tail
    float along = 0.5 - 0.5 * v.corner.y;
    float3 color = float3(0.55, 0.62, 0.8) * effect.params[1].z;
    return float4(color * across * along, 1.0);
}

// a flat quad on the surface where the drop last landed, its ring growing
[shader("vertex")]
RainVertex vs_ripples(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float since = float(effect.step) - p.custom.w;
    float t = since / float(RingSteps);
    bool showing = p.custom.w > NoLanding * 0.5 && t < 1.0;
    float radius = showing ? lerp(0.02, 0.2, t) : 0.0;
    float2 corner = stripCorner(vertexID);
    float3 world = p.custom.xyz +
                   float3(corner.x * radius, 0.012, corner.y * radius);

    RainVertex o;
    o.position = toClip(world);
    o.corner = corner;
    o.fade = showing ? (1.0 - t) * (1.0 - t) : 0.0;
    return o;
}

[shader("fragment")]
float4 fs_ripples(RainVertex v) : SV_Target {
    float reach = length(v.corner);
    float ring = saturate(1.0 - abs(reach - 0.8) / 0.15);
    float alpha = ring * v.fade * effect.params[1].w;
    return float4(0.75, 0.82, 0.95, alpha);
}

#include "ParticleKernel.slang"
