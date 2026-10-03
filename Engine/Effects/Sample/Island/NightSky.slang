#pragma once

// The night sky by direction; under the horizon it is the far sea, darker
// than the glow above it, so the horizon reads as a line.

#include "IslandScene.h"

static const float3 SkyZenith = float3(ISLAND_SKY_ZENITH);
static const float3 SkyHorizon = float3(ISLAND_SKY_HORIZON);
static const float3 FarSea = float3(ISLAND_FAR_SEA);
static const float3 MoonColor = float3(ISLAND_MOON_COLOR);
static const float3 MoonDisc = float3(ISLAND_MOON_DISC);

// withDisc false leaves the disc to a light whose glint already draws it
float3 nightSky(float3 dir, float3 toMoon, bool withDisc) {
    if(dir.y <= 0.0)
        return FarSea;

    // the glow stays near the horizon and gives way above about 20 deg
    float3 sky = lerp(SkyHorizon, SkyZenith, smoothstep(0.0, 0.35, dir.y));
    float toward = saturate(dot(dir, toMoon));
    sky += MoonColor * (0.04 * pow(toward, 96.0) + 0.012 * pow(toward, 8.0));
    if(withDisc) {
        float edge = cos(ISLAND_MOON_DISC_RADIUS);
        float rim = cos(ISLAND_MOON_DISC_RADIUS * 1.15);
        sky += MoonDisc * smoothstep(rim, edge, dot(dir, toMoon));
    }
    return sky;
}
