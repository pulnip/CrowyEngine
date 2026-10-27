#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <numbers>

#include <gtest/gtest.h>

#include "Kepler.hpp"
#include "LinearAlgebra.hpp"

using namespace Crowy;

namespace
{
    f64 LongitudeDeg(Vec3d p) {
        return NormalizeDegrees(toDegree(std::atan2(p.y, p.x)));
    }

    f64 Distance(Vec3d lhs, Vec3d rhs) {
        return norm(lhs - rhs);
    }

    f64 PerihelionDay(const OrbitalElements& el) {
        return (el.varpi - el.L0) / MeanMotionDegPerDay(el);
    }

    // both signs and two centuries either way, so nothing leans on t >= 0
    constexpr std::array<f64, 7>
        SAMPLE_DAYS{-73048.0, -6000.0, -1.5, 0.0, 1.5, 6000.0, 73048.0};
}

TEST(Kepler, SunHasNoOrbit) {
    const auto& sun = ORBIT_ELEMENTS[ORBIT_SUN_INDEX];

    EXPECT_EQ(MeanMotionDegPerDay(sun), 0.0);
    EXPECT_EQ(PeriodDays(sun), 0.0);

    // exactly zero: the frame interpolation has no branch for the Sun
    for(const auto day: SAMPLE_DAYS) {
        const auto p = OrbitPosition(ORBIT_SUN_INDEX, day);
        EXPECT_EQ(p.x, 0.0);
        EXPECT_EQ(p.y, 0.0);
        EXPECT_EQ(p.z, 0.0);
    }
}

TEST(Kepler, SunBorrowsEarthTrailLength) {
    EXPECT_EQ(
        TrailPeriodDays(ORBIT_SUN_INDEX),
        TrailPeriodDays(ORBIT_EARTH_INDEX)
    );

    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        EXPECT_EQ(TrailPeriodDays(b), PeriodDays(ORBIT_ELEMENTS[b]))
            << ORBIT_BODY_NAMES[b];
    }
}

TEST(Kepler, MeanMotionAndPeriodAgree) {
    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        const auto& el = ORBIT_ELEMENTS[b];

        EXPECT_NEAR(MeanMotionDegPerDay(el) * PeriodDays(el), 360.0, 1e-9)
            << ORBIT_BODY_NAMES[b];
    }
}

// the third-law periods must also land on the published sidereal ones
TEST(Kepler, DerivedPeriodsMatchPublished) {
    EXPECT_NEAR(PeriodDays(ORBIT_ELEMENTS[1]), 87.969, 0.05);  // Mercury
    EXPECT_NEAR(PeriodDays(ORBIT_ELEMENTS[2]), 224.701, 0.05); // Venus
    EXPECT_NEAR(PeriodDays(ORBIT_ELEMENTS[3]), 365.256, 0.05); // Earth
    EXPECT_NEAR(PeriodDays(ORBIT_ELEMENTS[4]), 686.980, 0.20); // Mars
    // the outer four drift more: the elements are J2000 means, not fits
    EXPECT_NEAR(PeriodDays(ORBIT_ELEMENTS[8]) / 365.25, 164.8, 0.5); // Neptune
}

TEST(Kepler, MeanAnomalyStaysNormalized) {
    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        for(const auto day: SAMPLE_DAYS) {
            const auto M = MeanAnomalyDeg(ORBIT_ELEMENTS[b], day);

            EXPECT_GE(M, -180.0) << ORBIT_BODY_NAMES[b] << " @ " << day;
            EXPECT_LT(M, 180.0) << ORBIT_BODY_NAMES[b] << " @ " << day;
        }
    }
}

// Mars and Neptune carry a negative L0
TEST(Kepler, MeanLongitudeWrapsFreely) {
    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        auto plus = ORBIT_ELEMENTS[b];
        plus.L0 += 360.0;
        auto minus = ORBIT_ELEMENTS[b];
        minus.L0 -= 720.0;

        for(const auto day: SAMPLE_DAYS) {
            const auto expected = OrbitPosition(ORBIT_ELEMENTS[b], day);

            EXPECT_LT(Distance(OrbitPosition(plus, day), expected), 1e-9)
                << ORBIT_BODY_NAMES[b] << " @ " << day;
            EXPECT_LT(Distance(OrbitPosition(minus, day), expected), 1e-9)
                << ORBIT_BODY_NAMES[b] << " @ " << day;
        }
    }
}

TEST(Kepler, NewtonConverges) {
    constexpr u32 STEPS = 2048;

    u32 worst = 0;
    CStr worstBody = ORBIT_BODY_NAMES[0];

    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        const auto& el = ORBIT_ELEMENTS[b];

        for(u32 s = 0; s < STEPS; ++s) {
            const auto M = (-std::numbers::pi_v<f64>)+2.0 *
                           std::numbers::pi_v<f64> * s / STEPS;
            const auto solved = SolveEccentricAnomaly(M, el.e);

            const auto residual =
                std::abs(solved.value - el.e * std::sin(solved.value) - M);
            EXPECT_LT(residual, KEPLER_TOLERANCE)
                << ORBIT_BODY_NAMES[b] << " M=" << M;
            ASSERT_LT(solved.iterations, KEPLER_MAX_ITERATIONS)
                << ORBIT_BODY_NAMES[b] << " M=" << M;

            if(solved.iterations > worst) {
                worst = solved.iterations;
                worstBody = ORBIT_BODY_NAMES[b];
            }
        }
    }

    // the loop cap exists for bad data, not for these bodies
    EXPECT_LE(worst, 6) << "worst body: " << worstBody;
    std::printf(
        "[ INFO     ] worst Newton iteration count: %u (%s)\n",
        worst,
        worstBody
    );
}

TEST(Kepler, RadiusStaysBetweenPerihelionAndAphelion) {
    constexpr u32 STEPS = 4096;

    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        const auto& el = ORBIT_ELEMENTS[b];
        const auto period = PeriodDays(el);

        const auto perihelion = el.a * (1.0 - el.e);
        const auto aphelion = el.a * (1.0 + el.e);

        auto minR = aphelion * 2.0;
        auto maxR = 0.0;
        for(u32 s = 0; s < STEPS; ++s) {
            const auto r = norm(OrbitPosition(el, period * s / STEPS));
            minR = std::min(minR, r);
            maxR = std::max(maxR, r);

            EXPECT_GE(r, perihelion - 1e-9) << ORBIT_BODY_NAMES[b];
            EXPECT_LE(r, aphelion + 1e-9) << ORBIT_BODY_NAMES[b];
        }

        // the sampling misses the extremes by a term quadratic in the step
        EXPECT_NEAR(minR, perihelion, perihelion * 1e-4) << ORBIT_BODY_NAMES[b];
        EXPECT_NEAR(maxR, aphelion, aphelion * 1e-4) << ORBIT_BODY_NAMES[b];
    }
}

TEST(Kepler, ClosesAfterOnePeriod) {
    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        const auto& el = ORBIT_ELEMENTS[b];
        const auto period = PeriodDays(el);

        for(const auto day: SAMPLE_DAYS) {
            EXPECT_LT(
                Distance(
                    OrbitPosition(el, day),
                    OrbitPosition(el, day + period)
                ),
                1e-9
            ) << ORBIT_BODY_NAMES[b]
              << " @ " << day;
        }
    }
}

TEST(Kepler, InclinationShowsUpInZ) {
    // Earth defines the ecliptic
    const auto& earth = ORBIT_ELEMENTS[ORBIT_EARTH_INDEX];
    ASSERT_EQ(earth.i, 0.0);

    constexpr u32 STEPS = 256;
    const auto earthPeriod = PeriodDays(earth);
    for(u32 s = 0; s < STEPS; ++s) {
        EXPECT_EQ(OrbitPosition(earth, earthPeriod * s / STEPS).z, 0.0);
    }

    // a rotation in the wrong order would flatten the orbit or overshoot
    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        if(b == ORBIT_EARTH_INDEX)
            continue;

        const auto& el = ORBIT_ELEMENTS[b];
        const auto period = PeriodDays(el);
        const auto limit = el.a * (1.0 + el.e) * std::sin(toRadian(el.i));

        auto peak = 0.0;
        for(u32 s = 0; s < STEPS; ++s) {
            peak = std::max(
                peak,
                std::abs(OrbitPosition(el, period * s / STEPS).z)
            );
        }

        EXPECT_LE(peak, limit + 1e-9) << ORBIT_BODY_NAMES[b];
        EXPECT_GT(peak, limit * 0.5) << ORBIT_BODY_NAMES[b];
    }
}

// The published row has varpi == Omega for Saturn; this fails if it is ever
// corrected.
TEST(Kepler, SaturnPerihelionSitsOnItsNode) {
    const auto& saturn = ORBIT_ELEMENTS[6];
    ASSERT_EQ(saturn.varpi, saturn.Omega);

    const auto p = OrbitPosition(saturn, PerihelionDay(saturn));
    EXPECT_NEAR(p.z, 0.0, 1e-9);

    const auto& mercury = ORBIT_ELEMENTS[1];
    const auto q = OrbitPosition(mercury, PerihelionDay(mercury));
    EXPECT_GT(std::abs(q.z), 0.01);
}

// At the March 2000 equinox the Sun is at ecliptic longitude 0 as seen from
// Earth, so Earth is at 180 as seen from the Sun.
TEST(Kepler, EarthReachesEquinoxOnSchedule) {
    // 2000-03-20 07:35 UTC, counted from J2000 (2000-01-01 12:00)
    constexpr f64 MARCH_EQUINOX_2000 = 78.816;

    const auto earth = OrbitPosition(ORBIT_EARTH_INDEX, MARCH_EQUINOX_2000);

    // the equinox is defined against the real perturbed Sun, not mean elements
    EXPECT_NEAR(std::abs(LongitudeDeg(earth)), 180.0, 0.2);

    const Vec3d sunFromEarth = Vec3d{} - earth;
    EXPECT_NEAR(LongitudeDeg(sunFromEarth), 0.0, 0.2);

    // the true longitude lags the mean one by the change in the equation of
    // centre, about 1.4 degrees across this arc
    const auto june = OrbitPosition(
        ORBIT_EARTH_INDEX,
        MARCH_EQUINOX_2000 + PeriodDays(ORBIT_ELEMENTS[ORBIT_EARTH_INDEX]) / 4
    );
    EXPECT_NEAR(LongitudeDeg(june), -90.0, 2.5);
}

TEST(Kepler, SampleOrbitsMatchesIndividualCalls) {
    std::array<Vec3d, ORBIT_BODY_COUNT> sampled{};

    for(const auto day: SAMPLE_DAYS) {
        SampleOrbits(day, sampled);

        for(u32 b = 0; b < ORBIT_BODY_COUNT; ++b) {
            EXPECT_EQ(Distance(sampled[b], OrbitPosition(b, day)), 0.0)
                << ORBIT_BODY_NAMES[b] << " @ " << day;
        }
    }
}

// the fixed-point phase is integer arithmetic the CPU runs exactly as the GPU
TEST(KeplerPhase, ReconstructsMeanAnomaly) {
    constexpr f64 DAY_PER_SAMPLE = 1.0;
    constexpr u32 STEPS = ORBIT_PHASE_MAX_STEPS;

    f64 worstDeg = 0.0;
    CStr worstBody = ORBIT_BODY_NAMES[0];

    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        const auto& el = ORBIT_ELEMENTS[b];
        // far enough out that a float mean anomaly is already hopeless
        const f64 startDay = 60000.0;
        const auto phase = MakePhaseGPU(el, startDay, DAY_PER_SAMPLE);

        for(u32 i = 0; i < STEPS; i += 337) {
            const auto expected =
                MeanAnomalyDeg(el, startDay + i * DAY_PER_SAMPLE);
            const auto actual = toDegree(PhaseToRadians(phase, i));

            const auto delta = std::abs(NormalizeDegrees(actual - expected));
            if(delta > worstDeg) {
                worstDeg = delta;
                worstBody = ORBIT_BODY_NAMES[b];
            }
        }
    }

    std::printf(
        "[ INFO     ] worst fixed-point phase error: %.3e deg (%s)\n",
        worstDeg,
        worstBody
    );
    // half an ulp of perStep per product, two products, never accumulating
    EXPECT_LT(worstDeg, 1e-4);
}

// pins the difference so nobody "simplifies" the split away
TEST(KeplerPhase, BlockSplitBeatsFlatStepping) {
    const auto& el = ORBIT_ELEMENTS[1]; // Mercury, the fastest phase
    const auto phase = MakePhaseGPU(el, 0.0, 1.0);

    constexpr u32 INDEX = ORBIT_PHASE_MAX_STEPS - 1;

    const auto expected = MeanAnomalyDeg(el, INDEX);
    const auto split = toDegree(PhaseToRadians(phase, INDEX));
    const auto flat = 360.0 *
                      static_cast<f64>(
                          static_cast<i32>(phase.phase0 + phase.perStep * INDEX)
                      ) /
                      4294967296.0;

    const auto splitError = std::abs(NormalizeDegrees(split - expected));
    const auto flatError = std::abs(NormalizeDegrees(flat - expected));

    std::printf(
        "[ INFO     ] phase error at %u steps: split %.3e deg, flat %.3e deg\n",
        INDEX,
        splitError,
        flatError
    );
    EXPECT_LT(splitError, flatError);
}

TEST(KeplerPhase, ElementsGPUMatchTheTable) {
    for(u32 b = 1; b < ORBIT_BODY_COUNT; ++b) {
        const auto& el = ORBIT_ELEMENTS[b];
        const auto gpu = MakeElementsGPU(el);

        EXPECT_FLOAT_EQ(gpu.a, static_cast<f32>(el.a)) << ORBIT_BODY_NAMES[b];
        EXPECT_FLOAT_EQ(gpu.e, static_cast<f32>(el.e)) << ORBIT_BODY_NAMES[b];

        EXPECT_NEAR(
            gpu.cosPeri * gpu.cosPeri + gpu.sinPeri * gpu.sinPeri,
            1.0f,
            1e-6f
        );
        EXPECT_NEAR(
            gpu.cosInc * gpu.cosInc + gpu.sinInc * gpu.sinInc,
            1.0f,
            1e-6f
        );
        EXPECT_NEAR(
            gpu.cosNode * gpu.cosNode + gpu.sinNode * gpu.sinNode,
            1.0f,
            1e-6f
        );
    }
}
