#include "PaintBrushProfile.hpp"

#include <algorithm>
#include <cmath>

#include "MintRandom.hpp"

namespace Crowy
{
    namespace
    {
        // UE_KINDA_SMALL_NUMBER
        constexpr f32 KindaSmall = 1e-4f;

        bool isNearlyZero(DVec3 v) {
            return std::abs(v.x) <= KindaSmall && std::abs(v.y) <= KindaSmall &&
                   std::abs(v.z) <= KindaSmall;
        }
    }

    PaintBrushProfile PaintBrushProfile::Paintball() noexcept {
        return PaintBrushProfile{
            .baseRadius = 12.0f,
            .maxRadius = 600.0f,
            .maxStretch = 4.0f,
            .minAlignedStretch = 1.5f
        };
    }

    PaintBrushProfile PaintBrushProfile::MopT() noexcept {
        return PaintBrushProfile{
            .baseRadius = 60.0f,
            .radiusPerSpeed = 0.0f,
            .maxRadius = 1200.0f,
            .maxStretch = 2.6667f,
            .minAlignedStretch = 1.5f
        };
    }

    PaintBrushProfile PaintBrushProfile::Smooth() noexcept {
        return PaintBrushProfile{
            .baseRadius = 50.0f,
            .maxRadius = 600.0f,
            .maxStretch = 4.0f,
            .minAlignedStretch = 1.5f,
            .shapeNoise = 0.0f
        };
    }

    f32 PaintBrushProfile::ComputeRadius(f32 volume, f32 speed) const noexcept {
        return std::min(
            baseRadius * std::sqrt(std::max(volume, 0.0f)) +
                radiusPerSpeed * speed,
            maxRadius
        );
    }

    PaintSplat PaintBrushProfile::BuildSplat(
        const PaintHit& hit,
        DVec3 incidentVelocity,
        u8 paintId,
        f32 volume,
        f32 heightAdd,
        i32 seed
    ) const {
        const auto normal = getSafeNormal(hit.impactNormal);
        const auto speed = static_cast<f32>(size(incidentVelocity));
        const auto incident =
            speed > KindaSmall ? incidentVelocity / speed : -normal;

        const auto cosTheta = static_cast<f32>(std::abs(dot(incident, normal)));

        const auto radius = ComputeRadius(volume, speed);
        const auto stretch =
            std::clamp(1.0f / std::max(cosTheta, KindaSmall), 1.0f, maxStretch);
        const auto tangent =
            getSafeNormal(incident - dot(incident, normal) * normal);
        // a grazing hit lands "ahead" of the contact along the tangent
        const auto centerShift = radius * (stretch - 1.0f) * CenterShiftScale();

        PaintSplat splat;
        // the shader's sin hash loses precision past 16 bits; the same value
        // drives the rotation below, so the shape follows one number
        splat.seed = static_cast<u16>(seed & 0xFFFF);

        // a near-round stamp gains nothing from aligning, which would repeat
        // one orientation every click: its rotation comes from the seed
        auto axisU = tangent;
        if(stretch < minAlignedStretch || isNearlyZero(axisU)) {
            RandomStream stream(splat.seed);
            const auto reference = std::abs(normal.z) < 0.9f
                                       ? DVec3{0.0, 0.0, 1.0}
                                       : DVec3{1.0, 0.0, 0.0};
            const auto base = getSafeNormal(cross(normal, reference));
            axisU = rotateAngleAxis(
                base,
                static_cast<f64>(stream.FRand() * 360.0f),
                normal
            );
        }

        splat.location = hit.impactPoint + tangent * centerShift;
        splat.normal = normal;
        splat.axisU = axisU;
        splat.radius = radius;
        splat.stretch = stretch;
        // the centre slid ahead, so in stamp space the impact sits behind the
        // origin: its u, by the long axis, anchors the spike field
        splat.impactU = -centerShift / std::max(radius * stretch, KindaSmall);
        splat.paintId = paintId;
        splat.heightAdd = heightAdd;
        splat.shapeNoise = shapeNoise;

        return splat;
    }
}
