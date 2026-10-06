#pragma once

#include "MintFrame.hpp"
#include "PaintTypes.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // a trace's answer: where and which way the surface faced
    struct PaintHit {
        DVec3 impactPoint;
        DVec3 impactNormal{0.0, 0.0, 1.0};
        // the surface hit, an index into the world's
        i64 surface = -1;
    };

    // UPaintBrushProfile: how one contact becomes a stamp. The surface that
    // receives it owns no brush tuning at all.
    struct PaintBrushProfile {
        // cm at a volume of 1, still
        f32 baseRadius = 25.0f;
        // cm added per cm/s of impact speed
        f32 radiusPerSpeed = 0.005f;
        f32 maxRadius = 120.0f;
        f32 maxStretch = 3.0f;
        // below it a near-round stamp spins from the seed instead of aligning
        f32 minAlignedStretch = 1.2f;
        // how far ahead of the contact a grazing stamp lands, % of the slide
        f32 centerShiftPercent = 50.0f;
        // BrushShapeNoise of the brush's material: 1 M_PaintBrush, 0 the
        // smooth instance
        f32 shapeNoise = 1.0f;

        // the C++ class defaults
        static PaintBrushProfile Default() noexcept { return {}; }
        // DA_Brush_Paintball: the splash droplets' marks
        static PaintBrushProfile Paintball() noexcept;
        // DA_Brush_Mop_T: the shipped fan gun's impact
        static PaintBrushProfile MopT() noexcept;
        // DA_Brush_Rainbow on MI_PaintBrushSmooth: a clean ellipse
        static PaintBrushProfile Smooth() noexcept;

        f32 ComputeRadius(f32 volume, f32 speed) const noexcept;
        f32 CenterShiftScale() const noexcept {
            return centerShiftPercent * 0.01f;
        }

        PaintSplat BuildSplat(
            const PaintHit& hit,
            DVec3 incidentVelocity,
            u8 paintId,
            f32 volume,
            f32 heightAdd,
            i32 seed
        ) const;
    };
}
