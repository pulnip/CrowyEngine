#include <cmath>
#include "Assert.hpp"
#include "Kepler.hpp"
#include "LinearAlgebra.hpp"

namespace{
    // one turn of the 32-bit fixed-point phase
    constexpr Crowy::f64 TURN_FIXED_SCALE = 4294967296.0;

    // wraps into [0, 1) turns first, so the cast never overflows whatever the
    // caller's absolute angle was
    Crowy::u32 turnsToFixed(Crowy::f64 turns) noexcept{
        const auto frac = turns - std::floor(turns);

        return static_cast<Crowy::u32>(
            static_cast<Crowy::u64>(std::llround(frac * TURN_FIXED_SCALE)) &
            0xFFFFFFFFull
        );
    }

    // "no orbit in this frame" - only the Sun row carries it
    constexpr bool IsDegenerate(const Crowy::OrbitalElements& el) noexcept{
        return el.a <= 0.0;
    }
}

namespace Crowy
{
    f64 norm(Vec3d v) noexcept{
        return std::sqrt(normSquared(v));
    }

    f64 NormalizeDegrees(f64 deg) noexcept{
        f64 wrapped = std::fmod(deg + 180.0, 360.0);
        if(wrapped < 0.0)
            wrapped += 360.0;

        return wrapped - 180.0;
    }

    f64 MeanMotionDegPerDay(const OrbitalElements& el) noexcept{
        if(IsDegenerate(el))
            return 0.0;

        return GAUSS_DEG_PER_DAY / std::pow(el.a, 1.5);
    }

    f64 PeriodDays(const OrbitalElements& el) noexcept{
        const auto n = MeanMotionDegPerDay(el);

        return n > 0.0 ? 360.0 / n : 0.0;
    }

    f64 TrailPeriodDays(u32 bodyIdx) noexcept{
        CROWY_ASSERT(bodyIdx < ORBIT_BODY_COUNT);

        // the Sun's apparent circle is Earth's orbit seen from the other end
        const auto source = bodyIdx == ORBIT_SUN_INDEX ?
            ORBIT_EARTH_INDEX :
            bodyIdx;

        return PeriodDays(ORBIT_ELEMENTS[source]);
    }

    f64 MeanAnomalyDeg(const OrbitalElements& el, f64 day) noexcept{
        // fmod inside NormalizeDegrees is exact for anything double can hold,
        // so a large |day| costs accuracy only through n*day itself
        return NormalizeDegrees(el.L0 + MeanMotionDegPerDay(el) * day - el.varpi);
    }

    EccentricAnomaly SolveEccentricAnomaly(f64 meanAnomaly, f64 e) noexcept{
        CROWY_ASSERT(0.0 <= e && e < 1.0);

        // one step of the standard series ahead of E = M; for the eccentricities
        // in ORBIT_ELEMENTS this lands close enough that Newton needs a handful
        // of steps even at Mercury
        f64 E = meanAnomaly + e * std::sin(meanAnomaly);

        u32 iterations = 0;
        while(iterations < KEPLER_MAX_ITERATIONS){
            const auto residual = E - e * std::sin(E) - meanAnomaly;
            if(std::abs(residual) < KEPLER_TOLERANCE)
                break;

            // 1 - e*cos(E) is the derivative and never vanishes for e < 1
            E -= residual / (1.0 - e * std::cos(E));
            ++iterations;
        }

        return EccentricAnomaly{
            .value = E,
            .iterations = iterations
        };
    }

    Vec3d OrbitPosition(const OrbitalElements& el, f64 day) noexcept{
        if(IsDegenerate(el))
            return Vec3d{};

        const auto M = toRadian(MeanAnomalyDeg(el, day));
        const auto E = SolveEccentricAnomaly(M, el.e).value;

        // in the orbital plane, perihelion on +x
        const auto xOrb = el.a * (std::cos(E) - el.e);
        const auto yOrb = el.a * std::sqrt(1.0 - el.e*el.e) * std::sin(E);

        // Rz(Omega) * Rx(i) * Rz(varpi - Omega), applied in that order.
        // varpi is measured along two different planes by construction, so the
        // in-plane angle is varpi - Omega, not varpi.
        const auto omega = toRadian(el.varpi - el.Omega);
        const auto cosOmega = std::cos(omega);
        const auto sinOmega = std::sin(omega);

        // Rz(varpi - Omega)
        const auto xNode = xOrb*cosOmega - yOrb*sinOmega;
        const auto yNode = xOrb*sinOmega + yOrb*cosOmega;

        // Rx(i)
        const auto inc = toRadian(el.i);
        const auto cosInc = yNode * std::cos(inc);
        const auto sinInc = yNode * std::sin(inc);

        // Rz(Omega)
        const auto node = toRadian(el.Omega);
        const auto cosNode = std::cos(node), sinNode = std::sin(node);

        return Vec3d{
            .x = xNode*cosNode - cosInc*sinNode,
            .y = xNode*sinNode + cosInc*cosNode,
            .z = sinInc
        };
    }

    Vec3d OrbitPosition(u32 bodyIdx, f64 day) noexcept{
        CROWY_ASSERT(bodyIdx < ORBIT_BODY_COUNT);

        return OrbitPosition(ORBIT_ELEMENTS[bodyIdx], day);
    }

    void SampleOrbits(f64 day, std::span<Vec3d, ORBIT_BODY_COUNT> out) noexcept{
        for(u32 b=0; b<ORBIT_BODY_COUNT; ++b){
            out[b] = OrbitPosition(ORBIT_ELEMENTS[b], day);
        }
    }

    OrbitElementsGPU MakeElementsGPU(const OrbitalElements& el) noexcept{
        const auto peri = toRadian(el.varpi - el.Omega);
        const auto inc = toRadian(el.i);
        const auto node = toRadian(el.Omega);

        return OrbitElementsGPU{
            .a = static_cast<f32>(el.a),
            .e = static_cast<f32>(el.e),
            .cosPeri = static_cast<f32>(std::cos(peri)),
            .sinPeri = static_cast<f32>(std::sin(peri)),
            .cosInc = static_cast<f32>(std::cos(inc)),
            .sinInc = static_cast<f32>(std::sin(inc)),
            .cosNode = static_cast<f32>(std::cos(node)),
            .sinNode = static_cast<f32>(std::sin(node))
        };
    }

    OrbitPhaseGPU MakePhaseGPU(
        const OrbitalElements& el,
        f64 day,
        f64 dayPerSample
    ) noexcept{
        CROWY_ASSERT(dayPerSample > 0.0);

        const auto n = MeanMotionDegPerDay(el);
        const auto perStep = n * dayPerSample / 360.0;

        return OrbitPhaseGPU{
            .phase0 = turnsToFixed((el.L0 + n * day - el.varpi) / 360.0),
            .perStep = turnsToFixed(perStep),
            .perBlock = turnsToFixed(perStep * ORBIT_PHASE_BLOCK)
        };
    }

    f64 PhaseToRadians(const OrbitPhaseGPU& phase, u32 index) noexcept{
        // unsigned overflow is the whole point: it reduces mod a full turn for
        // free, and it is well defined on both sides
        const u32 fixed = phase.phase0 +
            phase.perBlock * (index >> ORBIT_PHASE_BLOCK_SHIFT) +
            phase.perStep * (index & (ORBIT_PHASE_BLOCK - 1));

        // reinterpreting as signed lands it in (-0.5, 0.5] turns, which is the
        // range Newton wants to start from
        const auto turns = static_cast<f64>(static_cast<i32>(fixed)) /
            TURN_FIXED_SCALE;

        return toRadian(turns * 360.0);
    }
}
