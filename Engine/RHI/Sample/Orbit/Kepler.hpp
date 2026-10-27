#pragma once

#include <array>
#include <span>
#include <stdexcept>

#include "Primitives.hpp"

namespace Crowy
{
    // J2000 ecliptic frame (XY ecliptic, +Z north), AU, days since J2000;
    // degrees at the interface because element tables are published in them.

    inline constexpr u32 ORBIT_BODY_COUNT = 9;
    inline constexpr u32 ORBIT_SUN_INDEX = 0;
    inline constexpr u32 ORBIT_EARTH_INDEX = 3;

    struct Vec3d {
        f64 x = 0.0, y = 0.0, z = 0.0;

        constexpr explicit operator Vec3() const noexcept {
            return Vec3{
                static_cast<f32>(x),
                static_cast<f32>(y),
                static_cast<f32>(z)
            };
        }
    };

    inline constexpr Vec3d operator-(Vec3d lhs, Vec3d rhs) noexcept {
        return Vec3d{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }
    inline constexpr f64 dot(Vec3d lhs, Vec3d rhs) noexcept {
        return lhs.x * rhs.x + lhs.y * rhs.y + lhs.z * rhs.z;
    }
    inline constexpr f64 normSquared(Vec3d v) noexcept {
        return dot(v, v);
    }
    f64 norm(Vec3d v) noexcept;

    struct OrbitalElements {
        f64 a;     // semi-major axis (AU)
        f64 e;     // eccentricity
        f64 i;     // inclination (deg)
        f64 varpi; // longitude of perihelion (deg)
        f64 Omega; // longitude of the ascending node (deg)
        f64 L0;    // mean longitude at J2000 (deg)
    };

    struct OrbitBody {
        CStr name;
        OrbitalElements elements;
    };

    // J2000 mean elements; a = 0 marks the Sun. Mean motion comes from Kepler's
    // third law so each orbit closes on its own period.
    inline constexpr std::array<OrbitBody, ORBIT_BODY_COUNT> ORBIT_BODIES{
        OrbitBody{"Sun", {0.0, 0.0, 0.0, 0.0, 0.0, 0.0}},
        OrbitBody{
            "Mercury",
            {0.38709927, 0.20563593, 7.00498, 77.45780, 48.33077, 252.25032}
        },
        OrbitBody{
            "Venus",
            {0.72333566, 0.00677672, 3.39468, 131.60247, 76.67984, 181.97910}
        },
        OrbitBody{
            "Earth",
            {1.00000261, 0.01671123, 0.00000, 102.93768, 0.00000, 100.46457}
        },
        OrbitBody{
            "Mars",
            {1.52371034, 0.09339410, 1.84969, -23.94363, 49.55954, -4.55343}
        },
        OrbitBody{
            "Jupiter",
            {5.20288700, 0.04838624, 1.30440, 14.72848, 100.47391, 34.39644}
        },
        // varpi == Omega is the published value, not a transcription slip
        OrbitBody{
            "Saturn",
            {9.53667594, 0.05386179, 2.48599, 113.66242, 113.66242, 49.95424}
        },
        OrbitBody{
            "Uranus",
            {19.18916464, 0.04725744, 0.77264, 170.95428, 74.01693, 313.23810}
        },
        OrbitBody{
            "Neptune",
            {30.06992276, 0.00859048, 1.77004, 44.96476, 131.78423, -55.12003}
        }
    };
    static_assert(StrView{ORBIT_BODIES[ORBIT_SUN_INDEX].name} == "Sun");
    static_assert(StrView{ORBIT_BODIES[ORBIT_EARTH_INDEX].name} == "Earth");

    inline constexpr u32 OrbitBodyIndex(StrView name) {
        for(u32 b = 0; b < ORBIT_BODY_COUNT; ++b) {
            if(ORBIT_BODIES[b].name == name)
                return b;
        }

        throw std::invalid_argument("no such orbit body");
    }

    // Gaussian mean motion at a = 1 AU, deg/day
    inline constexpr f64 GAUSS_DEG_PER_DAY = 0.9856076686;

    inline constexpr f64 KEPLER_TOLERANCE = 1e-12;
    inline constexpr u32 KEPLER_MAX_ITERATIONS = 16;

    struct EccentricAnomaly {
        f64 value = 0.0;
        u32 iterations = 0;
    };

    // wraps to [-180, 180)
    f64 NormalizeDegrees(f64 deg) noexcept;

    f64 MeanMotionDegPerDay(const OrbitalElements&) noexcept;
    f64 PeriodDays(const OrbitalElements&) noexcept;
    // the Sun borrows Earth's period: geocentrically it traces Earth's orbit
    f64 TrailPeriodDays(u32 bodyIdx) noexcept;

    f64 MeanAnomalyDeg(const OrbitalElements&, f64 day) noexcept;

    EccentricAnomaly SolveEccentricAnomaly(f64 meanAnomaly, f64 e) noexcept;

    Vec3d OrbitPosition(const OrbitalElements&, f64 day) noexcept;
    Vec3d OrbitPosition(u32 bodyIdx, f64 day) noexcept;

    void SampleOrbits(f64 day, std::span<Vec3d, ORBIT_BODY_COUNT> out) noexcept;

    // The CPU keeps the clock: at 60,000 days a float mean anomaly has no
    // precision left, so the GPU gets fixed-point turns (one turn is 2^32).
    inline constexpr u32 ORBIT_PHASE_BLOCK_SHIFT = 8;
    inline constexpr u32 ORBIT_PHASE_BLOCK = 1u << ORBIT_PHASE_BLOCK_SHIFT;
    inline constexpr u32 ORBIT_PHASE_MAX_STEPS =
        ORBIT_PHASE_BLOCK * ORBIT_PHASE_BLOCK;

    // mirrors OrbitElements in OrbitKepler.slang
    struct OrbitElementsGPU {
        f32 a;
        f32 e;
        f32 cosPeri, sinPeri;
        f32 cosInc, sinInc;
        f32 cosNode, sinNode;
    };
    static_assert(sizeof(OrbitElementsGPU) == 32);

    // Mirrors OrbitPhase in OrbitKepler.slang. The block split keeps perStep's
    // rounding from piling up: 0.003 degrees over 65,536 samples unsplit.
    struct OrbitPhaseGPU {
        u32 phase0;
        u32 perStep;
        u32 perBlock;
        u32 _pad0 = 0;
    };
    static_assert(sizeof(OrbitPhaseGPU) == 16);

    OrbitElementsGPU MakeElementsGPU(const OrbitalElements&) noexcept;

    OrbitPhaseGPU MakePhaseGPU(
        const OrbitalElements&,
        f64 day,
        f64 dayPerSample
    ) noexcept;

    f64 PhaseToRadians(const OrbitPhaseGPU&, u32 index) noexcept;
}
