// Rain over the island: drops fall through a box, and where one meets the
// sea or the sand it leaves a ring that spreads and fades; the tipi keeps
// them off its floor.

#include "Effect.slang"
#include "IslandShapes.slang"

// what the desc's emitter and params mean here
float3 boxCenter() { return effect.emitter.xyz; }
float boxHalfHeight() { return effect.emitter.w; }
float2 boxHalfExtent() { return effect.params[0].xy; }
float fallSpeed() { return effect.params[0].z; }
float2 wind() { return float2(effect.params[0].w, effect.params[1].x); }
float streakSeconds() { return effect.params[1].y; }
float streakBrightness() { return effect.params[1].z; }
float ringOpacity() { return effect.params[1].w; }

// how long a ring spreads
static const uint RingSteps = 36u;
// custom.w before a drop's first landing
static const float NoLanding = -1.0e6;

// the sea at 0, the sand's crown, or the tipi's pyramid, which stops the
// rain over its open facets and its smoke hole too
float surfaceHeight(float2 xz) {
    return max(max(0.0, islandCrown(xz)), tipiRoof(xz));
}

// a drop in the box: at any height when first born, at the top after
float3 dropPosition(uint slot, uint generation, bool anywhere) {
    float2 extent = boxHalfExtent();
    float x = (particleRandom(slot, generation, 1u) * 2.0 - 1.0) * extent.x;
    float z = (particleRandom(slot, generation, 2u) * 2.0 - 1.0) * extent.y;
    float y = anywhere
        ? (particleRandom(slot, generation, 3u) * 2.0 - 1.0) * boxHalfHeight()
        : boxHalfHeight();
    return boxCenter() + float3(x, y, z);
}

Particle spawn(uint slot, uint generation) {
    Particle p = (Particle)0;
    p.position = dropPosition(slot, generation, true);
    p.velocity = float3(wind().x, -fallSpeed(), wind().y);
    // a drop is reborn where it lands, never by age
    p.lifeSteps = EventLife;
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
    // a ring's half width, in the quad's units
    float band : TEXCOORD2;
};

// a thin quad from the drop back along its fall, turned to the camera
[shader("vertex")]
RainVertex vs_streaks(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float2 corner = stripCorner(vertexID);
    float3 axis = normalize(p.velocity);
    float3 toCamera = cameraPosition.xyz - p.position;
    float away = length(toCamera);
    float3 side = normalize(cross(axis, toCamera));
    float streak = length(p.velocity) * streakSeconds();
    // never under half a pixel wide, dimmed for what it was widened by
    float halfWidth = max(p.size, 0.5 * ISLAND_PIXEL_ANGLE * away);
    float3 world = p.position + side * (corner.x * halfWidth) -
                   axis * ((corner.y * 0.5 + 0.5) * streak);

    RainVertex o;
    o.position = toClip(world);
    o.corner = corner;
    // far drops fade out instead of sparkling over the stars
    o.fade = (1.0 - smoothstep(6.0, 16.0, away)) * p.size / halfWidth;
    o.band = 0.0;
    return o;
}

[shader("fragment")]
float4 fs_streaks(RainVertex v) : SV_Target {
    float across = 1.0 - abs(v.corner.x);
    // bright at the drop, faint at the tail
    float along = 0.5 - 0.5 * v.corner.y;
    float3 color = float3(0.55, 0.62, 0.8) * streakBrightness();
    return float4(color * across * along * v.fade, 1.0);
}

// a flat quad on the surface where the drop last landed, its ring growing
[shader("vertex")]
RainVertex vs_ripples(uint vertexID: SV_VertexID, uint slot: SV_InstanceID) {
    Particle p = loadParticle(slot);
    float since = float(effect.step) - p.custom.w;
    float t = since / float(RingSteps);
    // a drop the tipi stopped leaves no ring; one on the far sand a wider one
    float crown = islandCrown(p.custom.xz);
    bool onTipi = tipiRoof(p.custom.xz) > max(crown, 0.0);
    bool onSand = crown > 0.0;
    bool showing = p.custom.w > NoLanding * 0.5 && t < 1.0 && !onTipi;
    float radius = showing ? lerp(0.02, onSand ? 0.3 : 0.2, t) : 0.0;
    float2 corner = stripCorner(vertexID);
    float3 world = p.custom.xyz +
                   float3(corner.x * radius, 0.012, corner.y * radius);

    RainVertex o;
    o.position = toClip(world);
    o.corner = corner;
    o.fade = showing ? (1.0 - t) * (1.0 - t) : 0.0;
    o.band = onSand ? 0.25 : 0.15;
    return o;
}

[shader("fragment")]
float4 fs_ripples(RainVertex v) : SV_Target {
    float reach = length(v.corner);
    float ring = saturate(1.0 - abs(reach - 0.8) / v.band);
    float alpha = ring * v.fade * ringOpacity();
    return float4(0.75, 0.82, 0.95, alpha);
}

#include "ParticleKernel.slang"
