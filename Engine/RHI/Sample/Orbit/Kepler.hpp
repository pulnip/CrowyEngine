#pragma once

#include <array>
#include <span>
#include "Primitives.hpp"

namespace Crowy
{
    // Keplerian two-body orbits for OrbitFrameSample, solved on the CPU.
    //
    // Everything here lives in the J2000 ecliptic frame: the ecliptic is the
    // XY plane, +Z is the ecliptic north pole, the Sun is at the origin.
    // That is the astronomical convention, not the engine's camera convention -
    // the view matrix is where the two meet, and this file stays out of it.
    //
    // Angles are degrees at the interface (that is how element tables are
    // published) and radians inside. Distances are AU. Time is days since
    // J2000; negative days are valid and the tests exercise them.

    inline constexpr u32 ORBIT_BODY_COUNT = 9;
    inline constexpr u32 ORBIT_SUN_INDEX = 0;
    inline constexpr u32 ORBIT_EARTH_INDEX = 3;

    // The solve stays in double end to end. Trail storage narrows to Vec3
    // later; narrowing here would fold the loss into the element table.
    // Orbit-local for now - it moves to Core the day something else wants it.
    struct Vec3d{
        f64 x = 0.0, y = 0.0, z = 0.0;

        constexpr explicit operator Vec3() const noexcept{
            return Vec3{
                static_cast<f32>(x),
                static_cast<f32>(y),
                static_cast<f32>(z)
            };
        }
    };

    inline constexpr Vec3d operator-(Vec3d lhs, Vec3d rhs) noexcept{
        return Vec3d{lhs.x - rhs.x, lhs.y - rhs.y, lhs.z - rhs.z};
    }
    inline constexpr f64 dot(Vec3d lhs, Vec3d rhs) noexcept{
        return lhs.x*rhs.x + lhs.y*rhs.y + lhs.z*rhs.z;
    }
    inline constexpr f64 normSquared(Vec3d v) noexcept{
        return dot(v, v);
    }
    f64 norm(Vec3d v) noexcept;

    struct OrbitalElements{
        f64 a;      // semi-major axis (AU)
        f64 e;      // eccentricity
        f64 i;      // inclination (deg)
        f64 varpi;  // longitude of perihelion (deg)
        f64 Omega;  // longitude of the ascending node (deg)
        f64 L0;     // mean longitude at J2000 (deg)
    };

    // J2000 mean elements, good enough for a visualisation. Swapping in a JPL
    // table later changes these numbers and nothing else.
    //
    // Index 0 is the Sun: a = 0 marks "no orbit in this frame", and every entry
    // point below short-circuits on it. Its geocentric circle is not stored -
    // it falls out of the frame interpolation for free (see the design doc).
    //
    // Mean motion is NOT in the table. It comes from Kepler's third law
    // (MeanMotionDegPerDay), so the orbit closes exactly on its own period
    // instead of drifting against a separately published rate.
    inline constexpr std::array<OrbitalElements, ORBIT_BODY_COUNT> ORBIT_ELEMENTS{
        OrbitalElements{ 0.0,        0.0,        0.0,     0.0,       0.0,       0.0       }, // Sun
        OrbitalElements{ 0.38709927, 0.20563593, 7.00498,  77.45780,  48.33077, 252.25032 }, // Mercury
        OrbitalElements{ 0.72333566, 0.00677672, 3.39468, 131.60247,  76.67984, 181.97910 }, // Venus
        OrbitalElements{ 1.00000261, 0.01671123, 0.00000, 102.93768,   0.00000, 100.46457 }, // Earth
        OrbitalElements{ 1.52371034, 0.09339410, 1.84969, -23.94363,  49.55954,  -4.55343 }, // Mars
        OrbitalElements{ 5.20288700, 0.04838624, 1.30440,  14.72848, 100.47391,  34.39644 }, // Jupiter
        // varpi == Omega is suspect published data, not a transcription slip on
        // our side; it puts Saturn's perihelion exactly on its ascending node.
        // KeplerTest pins that consequence so the oddity stays visible.
        OrbitalElements{ 9.53667594, 0.05386179, 2.48599, 113.66242, 113.66242,  49.95424 }, // Saturn
        OrbitalElements{19.18916464, 0.04725744, 0.77264, 170.95428,  74.01693, 313.23810 }, // Uranus
        OrbitalElements{30.06992276, 0.00859048, 1.77004,  44.96476, 131.78423, -55.12003 }  // Neptune
    };

    inline constexpr std::array<CStr, ORBIT_BODY_COUNT> ORBIT_BODY_NAMES{
        "Sun", "Mercury", "Venus", "Earth", "Mars",
        "Jupiter", "Saturn", "Uranus", "Neptune"
    };

    // Gaussian mean motion at a = 1 AU, deg/day
    inline constexpr f64 GAUSS_DEG_PER_DAY = 0.9856076686;

    // Newton on E - e*sin(E) = M stops when the residual falls under this.
    // 1e-12 rad is ~2e-7 arcsec: far below what double can carry through the
    // rotations, so the tolerance never limits the result.
    inline constexpr f64 KEPLER_TOLERANCE = 1e-12;
    // a safety net, not a target - the tests pin the real iteration count
    inline constexpr u32 KEPLER_MAX_ITERATIONS = 16;

    struct EccentricAnomaly{
        // radians
        f64 value = 0.0;
        // Newton steps taken. Reaching KEPLER_MAX_ITERATIONS means the residual
        // never fell under KEPLER_TOLERANCE.
        u32 iterations = 0;
    };

    // wraps to [-180, 180)
    f64 NormalizeDegrees(f64 deg) noexcept;

    // deg/day, from Kepler's third law. 0 for the Sun.
    f64 MeanMotionDegPerDay(const OrbitalElements&) noexcept;
    // days for one revolution. 0 for the Sun.
    f64 PeriodDays(const OrbitalElements&) noexcept;
    // How long a trail has to be to show one full turn of this body.
    // The Sun borrows Earth's period: in the geocentric frame it traces
    // Earth's orbit, so that is the turn a viewer actually sees.
    f64 TrailPeriodDays(u32 bodyIdx) noexcept;

    // M = L0 + n*t - varpi, normalized to [-180, 180)
    f64 MeanAnomalyDeg(const OrbitalElements&, f64 day) noexcept;

    // meanAnomaly in radians; e in [0, 1)
    EccentricAnomaly SolveEccentricAnomaly(f64 meanAnomaly, f64 e) noexcept;

    // heliocentric ecliptic position in AU
    Vec3d OrbitPosition(const OrbitalElements&, f64 day) noexcept;
    Vec3d OrbitPosition(u32 bodyIdx, f64 day) noexcept;

    // One ring-buffer sample: every body at the same instant. The shared
    // instant is what makes the frame interpolation legal - see the design doc.
    void SampleOrbits(f64 day, std::span<Vec3d, ORBIT_BODY_COUNT> out) noexcept;
}
