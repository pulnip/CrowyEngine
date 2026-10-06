#pragma once

#include <optional>
#include <vector>

#include "MintFrame.hpp"
#include "PaintSceneQuery.hpp"
#include "PaintSplash.hpp"
#include "PaintTypes.hpp"

// The picture's half of a splash: what NS_PaintSplash flies and what its
// landing handler stamps. Every machine flies its own, nothing is sent, and
// the score never looks at it.
namespace Crowy
{
    // the flight's fixed step, so any frame rate lands the same droplets
    inline constexpr f64 PaintFlightSubstep = 1.0 / 240.0;
    inline constexpr i32 PaintFlightSubstepsPerTick = 4;
    // the emitter kills a droplet whose path outgrows maxTravel by this much
    inline constexpr f64 PaintFlightPathScale = 1.5;
    // the world's gravity, cm/s^2
    inline constexpr f32 PaintGravityZ = -980.0f;

    struct PaintFlyingDroplet {
        DVec3 position;
        DVec3 velocity;
        f32 radius = 0.0f;
        // the effect reports this as the landing's speed
        f32 launchSpeed = 0.0f;
        f64 age = 0.0;
        // the path length the emitter's kill measures
        f64 traveled = 0.0;
        PaintDropletGroup group = PaintDropletGroup::Forward;
        // the largest maxMarkDroplets fly in the effect and may mark; the
        // rest exist only in the blob's picture
        bool marks = false;
        bool alive = true;
    };

    struct PaintDropletLanding {
        DVec3 point;
        DVec3 normal{0.0, 0.0, 1.0};
        f32 launchSpeed = 0.0f;
        i64 surface = -1;
    };

    // one contact's droplets and the handler that books their landings
    struct PaintSplashFlight {
        DVec3 contact;
        DVec3 normal{0.0, 0.0, 1.0};
        // landings nearer the contact leave no mark: the splat covers them
        f32 markClearance = 0.0f;
        i32 marks = 0;
        i32 splashSeed = 0;
        u8 paintId = 0;
        PaintLockGens lockGens;
        i64 onlySurface = -1;
        std::vector<PaintFlyingDroplet> droplets;

        bool IsAlive() const noexcept;
    };

    // the droplets leave the effect's origin, a centimetre off the surface
    PaintSplashFlight launchSplash(
        const PaintSplashProfile& profile,
        const PaintSplashInput& input,
        f32 splatRadius,
        u8 paintId,
        PaintLockGens lockGens,
        i64 onlySurface
    );

    // one substep of exact linear drag; a droplet ends on the first surface
    // its path meets, and only one that marks reports where
    void stepFlight(
        PaintSplashFlight& flight,
        const PaintSplashProfile& profile,
        const PaintSceneQuery& scene,
        std::vector<PaintDropletLanding>& landings
    );

    // UPaintSplashLandingHandler: a landing becomes a round, draw-only mark,
    // or nothing inside the clearance or past the mark cap
    std::optional<PaintSplat> landingMark(
        PaintSplashFlight& flight,
        const PaintSplashProfile& profile,
        const PaintDropletLanding& landing
    );
}
