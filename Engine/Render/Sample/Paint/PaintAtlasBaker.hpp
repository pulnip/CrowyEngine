#pragma once

#include <bit>
#include <span>
#include <vector>

#include "MintFrame.hpp"
#include "PaintIslandLayout.hpp"
#include "Primitives.hpp"

// MintChoco's PaintAtlasBaker: which point of the mesh each atlas texel stands
// for, and how far it is from a surface edge, baked on the CPU
namespace Crowy
{
    // binary16, rounded to nearest even as FFloat16 does on x64
    inline constexpr u16 toHalf(f32 value) noexcept {
        const auto bits = std::bit_cast<u32>(value);
        const auto sign = static_cast<u16>((bits >> 16) & 0x8000u);
        const auto exponent = static_cast<i32>((bits >> 23) & 0xFFu);
        auto mantissa = bits & 0x7FFFFFu;

        if(exponent == 0xFF)
            return static_cast<u16>(sign | 0x7C00u | (mantissa ? 0x200u : 0u));

        auto e = exponent - 127 + 15;
        if(e >= 0x1F)
            return static_cast<u16>(sign | 0x7C00u);
        if(e <= 0) {
            if(e < -10)
                return sign;
            // subnormal: shift the implicit bit in, then round
            mantissa |= 0x800000u;
            const auto shift = static_cast<u32>(14 - e);
            auto half = mantissa >> shift;
            const auto rest = mantissa & ((1u << shift) - 1u);
            const auto halfway = 1u << (shift - 1u);
            if(rest > halfway || (rest == halfway && (half & 1u)))
                ++half;
            return static_cast<u16>(sign | half);
        }

        auto half = static_cast<u32>(e << 10) | (mantissa >> 13);
        const auto rest = mantissa & 0x1FFFu;
        if(rest > 0x1000u || (rest == 0x1000u && (half & 1u)))
            ++half;

        return static_cast<u16>(sign | half);
    }

    struct PaintAtlasBakeInput {
        std::vector<Vec3> positions;
        // vertex normals, or empty; they only settle which way a face faces
        std::vector<Vec3> normals;
        std::vector<u32> indices;
        Box3d localBounds;
        PaintIslandLayout layout;
        f32 edgeFadeTexels = PaintEdgeFadeTexels;
        f32 edgeFadeSeamFraction = PaintEdgeFadeSeamFraction;
    };

    struct PaintAtlasBakeOutput {
        // atlasSize^2 RGBA16F texels: xyz the bounds-normalized local
        // position, w 1 where a surface was found
        std::vector<u16> positions;
        // atlasSize^2 bytes: 255 well inside a surface, 0 at every edge and
        // wherever there is no surface
        std::vector<u8> edgeFade;
        i32 coveredTexels = 0;
    };

    namespace PaintAtlasBaker
    {
        // a texel with no surface; the brush un-normalizes it to a point far
        // outside the mesh, so no stamp ever reaches a gutter texel
        inline constexpr f32 EmptyPosition = -64.0f;

        // For every island the triangles are projected along its axis and
        // rasterized at texel centres, the outermost surface winning.
        // Texels no triangle reaches keep EmptyPosition and w 0.
        void rasterize(const PaintAtlasBakeInput& in, std::vector<Vec4>& out);

        // Chebyshev distance to the nearest edge as a 0..255 ramp over
        // fadeTexels. An edge is an uncovered texel, the atlas border, or a
        // neighbour whose position jumps by more than seamFraction.
        void computeEdgeFade(
            std::span<const Vec4> positions,
            i32 atlasSize,
            f32 fadeTexels,
            f32 seamFraction,
            std::vector<u8>& out
        );

        // rasterize, fade and pack to half precision; safe on any thread
        void bake(const PaintAtlasBakeInput& in, PaintAtlasBakeOutput& out);
    }
}
