#pragma once

#include "Noise.slang"

// GPU half of Engine/RHI/Sample/Terrain/TerrainDensity.hpp.
// density > 0 is solid; the isosurface is density == 0.

// Mirrors TerrainParams in Engine/RHI/Sample/Terrain/Terrain.hpp.
struct TerrainParams {
    uint seed;
    float freq;
    uint octaves;
    float heightBase;
    float heightAmp;
    float caveFreq;
    float caveThreshold;
    float caveStrength;
};

float terrainBaseHeight(float x, float z, TerrainParams params) {
    return params.heightBase + params.heightAmp * fbm(
        x * params.freq,
        z * params.freq,
        params.seed,
        params.octaves
    );
}

// spelled out to match the C++ side expression exactly
float terrainSmoothstep(float min, float max, float x) {
    float t = clamp((x - min) / (max - min), 0.0f, 1.0f);

    return t * t * (3.0f - 2.0f * t);
}

// fades at both ends so caves never breach the floor or the surface
float terrainCaveMask(float y, float baseHeight) {
    // caves fade out within this distance of the floor and the surface
    static const float TERRAIN_CAVE_FADE = 4.0f;

    float bedrockMask = terrainSmoothstep(
        0.0f,
        TERRAIN_CAVE_FADE,
        y
    );

    float crustBottom = baseHeight - TERRAIN_CAVE_FADE;
    float caveCeiling = crustBottom - TERRAIN_CAVE_FADE;
    float surfaceMask = 1.0f - terrainSmoothstep(
        caveCeiling,
        crustBottom,
        y
    );

    return bedrockMask * surfaceMask;
}

float terrainDensity(
    float3 position,
    float baseHeight,
    TerrainParams params
) {
    // decorrelates the cave field from the surface fbm
    static const uint TERRAIN_CAVE_SEED_MIX = 0x5f356495u;

    float solid = baseHeight - position.y;

    // remapped to [0, 1] so caveThreshold reads as a fraction
    float cave = 0.5f + 0.5f * perlin(
        position.x * params.caveFreq,
        position.y * params.caveFreq,
        position.z * params.caveFreq,
        params.seed ^ TERRAIN_CAVE_SEED_MIX
    );

    float carve = max(0.0f, cave - params.caveThreshold) *
        params.caveStrength * terrainCaveMask(position.y, baseHeight);

    return solid - carve;
}

float terrainDensity(
    float3 position,
    TerrainParams params
) {
    float baseHeight = terrainBaseHeight(position.x, position.z, params);

    return terrainDensity(
        position,
        baseHeight,
        params
    );
}
