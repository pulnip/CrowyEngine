#include "AsteroidBelt.hpp"

#include <array>
#include <cmath>

#include "Assert.hpp"

namespace
{
    using namespace Crowy;

    // Kirkwood gaps: semi-major axes in mean-motion resonance with Jupiter;
    // without them the belt is a featureless annulus.
    struct Gap {
        f64 centerAU;
        f64 halfWidthAU;
    };
    constexpr std::array<Gap, 4> KIRKWOOD_GAPS{
        Gap{2.502, 0.030}, // 3:1
        Gap{2.825, 0.022}, // 5:2
        Gap{2.958, 0.016}, // 7:3
        Gap{3.279, 0.030}  // 2:1
    };

    bool InGap(f64 a) {
        for(const auto& gap: KIRKWOOD_GAPS) {
            if(std::abs(a - gap.centerAU) < gap.halfWidthAU)
                return true;
        }

        return false;
    }

    // xorshift32
    class Rng {
    private:
        u32 state;

    public:
        explicit Rng(u32 seed)
            : state(seed != 0 ? seed : 1u) {}

        u32 Next() {
            state ^= state << 13;
            state ^= state >> 17;
            state ^= state << 5;

            return state;
        }

        f64 Unit() { return Next() * (1.0 / 4294967296.0); }
        f64 Range(f64 lo, f64 hi) { return lo + (hi - lo) * Unit(); }
    };
}

namespace Crowy
{
    AsteroidBelt::AsteroidBelt(u32 count, u32 seed) {
        Rng rng(seed);
        elements.reserve(count);

        while(elements.size() < count) {
            // biased inward: the real belt thins out towards Jupiter, and a
            // flat distribution reads as a solid band
            const auto t = rng.Unit();
            const auto a =
                BELT_INNER_AU + (BELT_OUTER_AU - BELT_INNER_AU) * t * t;
            if(InGap(a))
                continue;

            elements.push_back(
                OrbitalElements{
                    .a = a,
                    // most of the belt is under 0.2; a long tail would throw
                    // asteroids across Mars's orbit
                    .e = rng.Range(0.0, 0.22) * rng.Unit() + 0.02,
                    // real inclinations reach about 20 degrees; from above, the
                    // foreshortening makes the belt read as a torus
                    .i = rng.Range(0.0, 18.0) * rng.Unit(),
                    .varpi = rng.Range(0.0, 360.0),
                    .Omega = rng.Range(0.0, 360.0),
                    .L0 = rng.Range(0.0, 360.0)
                }
            );
        }
    }

    void AsteroidBelt::Sample(f64 day, std::span<Vec3> out) const {
        CROWY_ASSERT(out.size() >= elements.size());

        for(usize i = 0; i < elements.size(); ++i) {
            out[i] = static_cast<Vec3>(OrbitPosition(elements[i], day));
        }
    }
}
