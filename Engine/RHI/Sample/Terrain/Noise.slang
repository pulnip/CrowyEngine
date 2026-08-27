#pragma once

// GPU half of Engine/RHI/Sample/Terrain/Noise.hpp, laid out in the same
// order. TerrainDensityCheck compares both on every smoke run.

uint hash(uint x) {
    // Perlin's permutation table, replaced by an integer hash
    x ^= x >> 16;
    x *= 0x7feb352du;
    x ^= x >> 15;
    x *= 0x846ca68bu;
    x ^= x >> 16;

    return x;
}

uint hash(uint x, uint y, uint seed) {
    return hash(
        x * 0x8da6b343u +
        y * 0xd8163841u +
        seed * 0x9e3779b9u
    );
}

uint hash(uint x, uint y, uint z, uint seed) {
    return hash(
        x * 0x8da6b343u +
        y * 0xd8163841u +
        z * 0xcb1ab31fu +
        seed * 0x9e3779b9u
    );
}

// 8 unit-length directions: the 4 axes and the 4 diagonals
float grad(uint h, float x, float y) {
    const float D = 0.70710678f;

    switch (h & 7u) {
    case 0:  return  x;
    case 1:  return -x;
    case 2:  return  y;
    case 3:  return -y;
    case 4:  return D * ( x + y);
    case 5:  return D * (-x + y);
    case 6:  return D * ( x - y);
    default: return D * (-x - y);
    }
}

// Perlin's 12 cube-edge gradients, selected the way improved noise does
float grad(uint h, float x, float y, float z) {
    uint hh = h & 15u;
    float u = hh < 8u ? x : y;
    float v = hh < 4u ? y : ((hh == 12u || hh == 14u) ? x : z);

    return ((hh & 1u) == 0u ? u : -u) + ((hh & 2u) == 0u ? v : -v);
}

float fade(float t) {
    // for continuous second derivative
    return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

float perlin(float x, float y, uint seed) {
    uint ux = asuint(int(floor(x)));
    uint uy = asuint(int(floor(y)));

    float tx = x - ux;
    float ty = y - uy;
    float u = fade(tx);
    float v = fade(ty);

    float n00 = grad(hash(ux,     uy,     seed), tx,        ty);
    float n10 = grad(hash(ux + 1, uy,     seed), tx - 1.0f, ty);
    float n01 = grad(hash(ux,     uy + 1, seed), tx,        ty - 1.0f);
    float n11 = grad(hash(ux + 1, uy + 1, seed), tx - 1.0f, ty - 1.0f);

    // rescales the 1/sqrt2 span of unit gradients to [-1, 1]
    return 1.41421356f * lerp(
        lerp(n00, n10, u),
        lerp(n01, n11, u),
        v
    );
}

float perlin(float x, float y, float z, uint seed) {
    float fx = floor(x);
    float fy = floor(y);
    float fz = floor(z);
    uint ux = asuint(int(fx));
    uint uy = asuint(int(fy));
    uint uz = asuint(int(fz));

    float tx = x - fx;
    float ty = y - fy;
    float tz = z - fz;
    float u = fade(tx);
    float v = fade(ty);
    float w = fade(tz);

    float x0 = tx, x1 = tx - 1.0f;
    float y0 = ty, y1 = ty - 1.0f;
    float z0 = tz, z1 = tz - 1.0f;

    float n000 = grad(hash(ux,     uy,     uz,     seed), x0, y0, z0);
    float n100 = grad(hash(ux + 1, uy,     uz,     seed), x1, y0, z0);
    float n010 = grad(hash(ux,     uy + 1, uz,     seed), x0, y1, z0);
    float n110 = grad(hash(ux + 1, uy + 1, uz,     seed), x1, y1, z0);
    float n001 = grad(hash(ux,     uy,     uz + 1, seed), x0, y0, z1);
    float n101 = grad(hash(ux + 1, uy,     uz + 1, seed), x1, y0, z1);
    float n011 = grad(hash(ux,     uy + 1, uz + 1, seed), x0, y1, z1);
    float n111 = grad(hash(ux + 1, uy + 1, uz + 1, seed), x1, y1, z1);

    // no rescale in 3D, matching the reference implementation
    return lerp(
        lerp(lerp(n000, n100, u), lerp(n010, n110, u), v),
        lerp(lerp(n001, n101, u), lerp(n011, n111, u), v),
        w
    );
}

static const uint MAX_OCTAVES = 8;

float fbm(float x, float y, uint seed, uint octaves) {
    uint count = clamp(octaves, 1, MAX_OCTAVES);

    float sum = 0.0f;
    float amplitude = 1.0f;
    float amplitudeSum = 0.0f;
    float px = x, py = y;

    for (uint i = 0; i < count; ++i) {
        uint octaveSeed = seed + asuint(i) * 0x9e3779b9u;

        sum += amplitude * perlin(px, py, octaveSeed);
        amplitudeSum += amplitude;

        // gain 0.5, lacunarity 2
        amplitude *= 0.5f;
        px *= 2.0f;
        py *= 2.0f;
    }

    // normalize to [-1, 1]
    return sum / amplitudeSum;
}
