#pragma once

#include <algorithm>
#include <vector>

#include "MintFrame.hpp"
#include "PaintTypes.hpp"
#include "Primitives.hpp"

// FPaintIsland and FPaintIslandLayout: where every enabled direction's planar
// island sits in a surface's one square atlas
namespace Crowy
{
    // One planar island: the mesh seen along one local axis, packed into a
    // rectangle. The bake, the brush and the surface shader all go through
    // this mapping, so a surface point lands on the same texel for all three.
    // X sees (Y, Z), Y sees (X, Z), Z sees (X, Y); a negative direction uses
    // its axis's plane with the opposite depth sign.
    struct PaintIsland {
        PaintFaceDirection direction = PaintFaceDirection::Up;
        // the projection axis and the two plane axes, as component indices
        i32 axis = 2;
        i32 axisB = 0;
        i32 axisC = 1;
        // +1 keeps the surface with the largest coordinate along axis
        i32 sign = 1;
        // in atlas texels, gutter included
        IntRect rect;
        // the atlas texel the plane's normalized (0, 0) lands on
        IntPoint contentOrigin;
        // texels the plane's full extent spans on each axis
        Vec2d contentTexels;

        static void PlaneAxes(i32 axis, i32& axisB, i32& axisC) noexcept;

        // the fractional atlas texel of a bounds-normalized local position
        Vec2d ProjectNormalized(DVec3 normalized) const noexcept {
            return {
                contentOrigin.x + normalized[axisB] * contentTexels.x,
                contentOrigin.y + normalized[axisC] * contentTexels.y
            };
        }

        // (uv offset, uv scale): UV = xy + n.bc * zw
        Vec4 ToShaderParam(i32 atlasSize) const noexcept;
    };

    struct PaintIslandLayout {
        // enabled directions only, in enum order
        std::vector<PaintIsland> islands;
        // a power of two; 0 with no direction enabled
        i32 atlasSize = 0;
        // coarser than requested when the islands would not fit maxSize
        f32 texelCm = 0.0f;
        u8 enabledDirections = 0;

        // One island per enabled direction, spanning the mesh's world
        // footprint at requestedTexelCm plus padTexels on every side,
        // shelf-packed into the smallest power-of-two square in
        // [minSize, maxSize]; the texel grows until maxSize holds them.
        static PaintIslandLayout Build(
            const Box3d& localBounds,
            DVec3 scale3D,
            u8 enabledDirections,
            f32 requestedTexelCm,
            i32 padTexels,
            i32 minSize,
            i32 maxSize
        );

        const PaintIsland* Find(PaintFaceDirection direction) const noexcept;
        bool IsEmpty() const noexcept { return islands.empty(); }
        Str ToString() const;
    };

    // MintChoco's UPaintSettings defaults
    inline constexpr f32 PaintTexelSizeCm = 0.5f;
    inline constexpr i32 PaintIslandPaddingTexels = 8;
    inline constexpr i32 PaintMinRenderTargetSize = 256;
    inline constexpr i32 PaintMaxRenderTargetSize = 2048;
    inline constexpr f32 PaintEdgeFadeTexels = 8.0f;
    inline constexpr f32 PaintEdgeFadeSeamFraction = 0.05f;
    inline constexpr f32 PaintDistanceRange = 4.0f;
    // what a wall's top edge would claim without it
    inline constexpr f64 PaintAutoUpMinIslandArea = 2500.0;
    // the gutter: the edge fade and the distance channel both fit in it
    inline constexpr i32 PaintIslandPad = std::max(
        PaintIslandPaddingTexels,
        static_cast<i32>(PaintEdgeFadeTexels) +
            static_cast<i32>(PaintDistanceRange) + 1
    );
}
