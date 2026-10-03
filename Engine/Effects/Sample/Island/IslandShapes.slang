#pragma once

// The island's shapes as functions of xz, so rain lands where the meshes
// stand and a check can say so: the sand's crown and the tipi's pyramid.

#include "IslandScene.h"

static const float3 IslandRadii = float3(ISLAND_RADII);
static const float TipiFacetAngle = 6.2831853 / float(ISLAND_TIPI_FACETS);

// the sand's height, or the ellipsoid's centre where it does not reach
float islandCrown(float2 xz) {
    float2 q = xz / IslandRadii.xz;
    float inside = 1.0 - dot(q, q);
    return inside > 0.0
        ? ISLAND_CENTER_Y + IslandRadii.y * sqrt(inside)
        : ISLAND_CENTER_Y;
}

// facet k's outward direction across the ground
float2 tipiFacetNormal(uint k) {
    float azimuth =
        ISLAND_TIPI_FIRST_POLE + (float(k) + 0.5) * TipiFacetAngle;
    return float2(sin(azimuth), cos(azimuth));
}

// how far out xz lies across the facets, and across which one
float tipiReach(float2 xz, out uint facet) {
    float reach = dot(xz, tipiFacetNormal(0u));
    facet = 0u;
    for(uint k = 1u; k < ISLAND_TIPI_FACETS; ++k) {
        float across = dot(xz, tipiFacetNormal(k));
        if(across > reach) {
            reach = across;
            facet = k;
        }
    }
    return reach;
}

// how far the facet planes stand from the axis at height y
float tipiApothem(float y) {
    float base = ISLAND_TIPI_RADIUS * cos(0.5 * TipiFacetAngle);
    return base * (ISLAND_TIPI_APEX_Y - y) /
           (ISLAND_TIPI_APEX_Y - ISLAND_TIPI_BASE_Y);
}

// the pyramid the canvas lies on, up to the poles' crossing: the lowest of
// the facet planes, so it is continuous over the open facets and the hole
float tipiRoof(float2 xz) {
    uint facet;
    float reach = tipiReach(xz, facet);
    float base = ISLAND_TIPI_RADIUS * cos(0.5 * TipiFacetAngle);
    return ISLAND_TIPI_APEX_Y -
           (ISLAND_TIPI_APEX_Y - ISLAND_TIPI_BASE_Y) * reach / base;
}
