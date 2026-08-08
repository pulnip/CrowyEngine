#pragma once

#include <algorithm>
#include <cmath>
#include <numbers>
#include "Primitives.hpp"

namespace Crowy
{
    // CPU half of Engine/Shader/TerrainNoise.slang, laid out in the same order.
    // TerrainDensityCheck compares both on every smoke run.

    namespace detail{
        // Perlin's permutation table, replaced by an integer hash
        inline constexpr u32 hash(u32 x) noexcept{
            x ^= x >> 16;
            x *= 0x7feb352du;
            x ^= x >> 15;
            x *= 0x846ca68bu;
            x ^= x >> 16;

            return x;
        }

        inline constexpr u32 hash(u32 x, u32 y, u32 seed) noexcept{
            return hash(
                x * 0x8da6b343u +
                y * 0xd8163841u +
                seed * 0x9e3779b9u
            );
        }

        inline constexpr u32 hash(u32 x, u32 y, u32 z, u32 seed) noexcept{
            return hash(
                x * 0x8da6b343u +
                y * 0xd8163841u +
                z * 0xcb1ab31fu +
                seed * 0x9e3779b9u
            );
        }

        // 8 unit-length directions: the 4 axes and the 4 diagonals
        inline constexpr f32 grad(u32 h, f32 x, f32 y) noexcept{
            constexpr f32 D = std::numbers::sqrt2_v<f32> / 2;

            // (x, y) dot (dir determined by h)
            switch(h & 7u){
            // Axis aligned
            case 0:  return  x; // dir = ( 1,  0)
            case 1:  return -x; // dir = (-1,  0)
            case 2:  return  y; // dir = ( 0,  1)
            case 3:  return -y; // dir = ( 0, -1)
            // Diagonal
            case 4:  return D * ( x + y);
            case 5:  return D * (-x + y);
            case 6:  return D * ( x - y);
            default: return D * (-x - y);
            }
        }

        // Perlin's 12 cube-edge gradients, selected the way improved noise does
        inline constexpr f32 grad(u32 h, f32 x, f32 y, f32 z) noexcept{
            const u32 hh = h & 15u;
            const f32 u = hh < 8u ? x : y;
            const f32 v = hh < 4u ? y : (hh == 12u || hh == 14u ? x : z);

            return ((hh & 1u) == 0u ? u : -u) + ((hh & 2u) == 0u ? v : -v);
        }

        inline constexpr f32 fade(f32 t) noexcept{
            // for continuous second derivative
            return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
        }

        inline constexpr f32 lerp(f32 a, f32 b, f32 t) noexcept{
            return a + t * (b - a);
        }
    }

    inline f32 perlin(f32 x, f32 y, u32 seed) noexcept{
        using namespace detail;

        // (ux) --- (x)  ---  (ux+1)
        // | <- tx ->| <- 1-tx -> |
        // Notice. f32 -> u32 is undefined below zero
        const f32 ux = static_cast<u32>(static_cast<i32>(std::floor(x)));
        const f32 uy = static_cast<u32>(static_cast<i32>(std::floor(y)));
        const f32 tx = x - ux, ty = y - uy;

        // gradient (ux + ..., uy + ...) to (x, y)
        const f32 n00 = grad(hash(ux,     uy,     seed), tx,        ty);
        const f32 n10 = grad(hash(ux + 1, uy,     seed), tx - 1.0f, ty);
        const f32 n01 = grad(hash(ux,     uy + 1, seed), tx,        ty - 1.0f);
        const f32 n11 = grad(hash(ux + 1, uy + 1, seed), tx - 1.0f, ty - 1.0f);

        const f32 u = fade(tx);
        const f32 v = fade(ty);

        // rescales the 1/sqrt2 span of unit gradients to [-1, 1]
        return std::numbers::sqrt2_v<f32> * lerp(
            lerp(n00, n10, u),
            lerp(n01, n11, u),
            v
        );
    }
    inline f32 perlin(f32 x, f32 y, f32 z, u32 seed) noexcept{
        using namespace detail;

        const auto ux = static_cast<u32>(static_cast<i32>(std::floor(x)));
        const auto uy = static_cast<u32>(static_cast<i32>(std::floor(y)));
        const auto uz = static_cast<u32>(static_cast<i32>(std::floor(z)));

        const f32 tx = x - ux;
        const f32 ty = y - uy;
        const f32 tz = z - uz;

        const f32 x0 = tx, x1 = tx - 1.0f;
        const f32 y0 = ty, y1 = ty - 1.0f;
        const f32 z0 = tz, z1 = tz - 1.0f;

        const f32 n000 = grad(hash(ux,     uy,     uz,     seed), x0, y0, z0);
        const f32 n100 = grad(hash(ux + 1, uy,     uz,     seed), x1, y0, z0);
        const f32 n010 = grad(hash(ux,     uy + 1, uz,     seed), x0, y1, z0);
        const f32 n110 = grad(hash(ux + 1, uy + 1, uz,     seed), x1, y1, z0);
        const f32 n001 = grad(hash(ux,     uy,     uz + 1, seed), x0, y0, z1);
        const f32 n101 = grad(hash(ux + 1, uy,     uz + 1, seed), x1, y0, z1);
        const f32 n011 = grad(hash(ux,     uy + 1, uz + 1, seed), x0, y1, z1);
        const f32 n111 = grad(hash(ux + 1, uy + 1, uz + 1, seed), x1, y1, z1);

        const f32 u = fade(tx);
        const f32 v = fade(ty);
        const f32 w = fade(tz);

        // no rescale in 3D, matching the reference implementation
        return lerp(
            lerp(lerp(n000, n100, u), lerp(n010, n110, u), v),
            lerp(lerp(n001, n101, u), lerp(n011, n111, u), v),
            w
        );
    }

    inline constexpr u32 MAX_OCTAVES = 8;

    inline f32 fbm(f32 x, f32 y, u32 seed, u32 octaves) noexcept{
        const u32 count = std::clamp(octaves, 1u, MAX_OCTAVES);

        f32 sum = 0.0f;
        f32 amplitude = 1.0f;
        f32 amplitudeSum = 0.0f;
        f32 px = x, py = y;

        for(u32 i = 0; i < count; ++i){
            const u32 octaveSeed = seed + i * 0x9e3779b9u;

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
}
