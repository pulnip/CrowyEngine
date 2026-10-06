#pragma once

#include <span>
#include <vector>

#include "MintFrame.hpp"
#include "PaintBrushProfile.hpp"
#include "Primitives.hpp"

// MintChoco's splash as arithmetic (PaintSplash.h): how a contact turns into
// droplets and where they come down. The score and the picture both start
// from these droplets; they part ways only after.
namespace Crowy
{
    inline constexpr i32 PaintMaxDroplets = 16;

    // where a droplet heads relative to the ball's travel
    enum class PaintDropletGroup : u8 {
        Forward,
        Side,
        Back,
    };

    // FPaintSplashDropletGroup: one heading's share of the splash
    struct PaintSplashGroup {
        // half-width of the fan around the heading, degrees
        f32 spreadDeg = 45.0f;
        // off the surface normal: 0 leaves straight up, 90 skims
        f32 elevationMinDeg = 30.0f;
        f32 elevationMaxDeg = 65.0f;
        // launch speed over the approach speed
        f32 speedScaleMin = 0.08f;
        f32 speedScaleMax = 0.16f;
        // of the ball's tangential speed, what the droplet keeps
        f32 slideScale = 0.1f;
    };

    // UPaintSplashProfile, the class defaults
    struct PaintSplashProfile {
        i32 dropletCount = 16;
        // droplet radius over the ball's; the bias above 1 favours small ones
        f32 dropletRadiusScaleMin = 0.12f;
        f32 dropletRadiusScaleMax = 0.45f;
        f32 dropletRadiusBias = 2.0f;
        // the droplets together never hold more of the ball's volume
        f32 volumeFraction = 0.6f;
        // cm/s along the normal; slower only splats
        f32 minNormalSpeed = 400.0f;
        f32 tangentialLaunchShare = 0.5f;
        f32 maxDropletSpeed = 900.0f;
        f32 forwardShareHeadOn = 0.4f;
        f32 forwardShareGrazing = 0.75f;
        // of the droplets not thrown forward, the share thrown back
        f32 backShareOfRest = 0.3f;
        PaintSplashGroup forward{50.0f, 35.0f, 70.0f, 0.10f, 0.20f, 0.15f};
        PaintSplashGroup side{35.0f, 25.0f, 60.0f, 0.06f, 0.14f, 0.05f};
        PaintSplashGroup back{40.0f, 15.0f, 45.0f, 0.04f, 0.10f, 0.0f};
        f32 gravityScale = 1.0f;
        // per second; the picture's flight has it, the score's does not
        f32 drag = 0.4f;
        f32 maxLifetime = 1.8f;
        f32 maxTravel = 400.0f;
        PaintBrushProfile dropletBrush = PaintBrushProfile::Paintball();
        f32 dropletSplatVolume = 0.25f;
        f32 dropletHeightAdd = 0.2f;
        // the largest droplets that fly and mark
        i32 maxMarkDroplets = 8;
        // over the ball's own splat radius: landings inside leave no mark
        f32 markClearanceScale = 1.1f;
        f32 phantomCellRadiusScale = 1.0f;
        // the largest droplets whose phantom landings score
        i32 maxScoreDroplets = 4;

        // DA_Splash_Paintball, on the heavy paintballs
        static PaintSplashProfile Paintball() noexcept;

        // the radius of a droplet's mark landing at `speed`
        f32 ComputeMarkRadius(f32 speed) const noexcept {
            return dropletBrush.ComputeRadius(dropletSplatVolume, speed);
        }
    };

    struct PaintSplashInput {
        DVec3 impactPoint;
        DVec3 impactNormal{0.0, 0.0, 1.0};
        // cm/s, into the surface
        DVec3 incidentVelocity;
        f32 ballRadius = 6.0f;
        // the ball's; the splash derives its own stream from it
        i32 seed = 0;
    };

    struct PaintDroplet {
        DVec3 position;
        DVec3 velocity;
        f32 radius = 0.0f;
        PaintDropletGroup group = PaintDropletGroup::Forward;
    };

    // where a droplet comes back down on the contact's plane, and how fast
    struct PaintPhantomLanding {
        DVec3 point;
        f32 speed = 0.0f;
    };

    // the splash's stream, apart from the ball's so its splat is undisturbed
    i32 splashSeed(i32 ballSeed) noexcept;

    // the approach along the normal (positive into the surface) and what
    // is left in the surface plane
    void splitVelocity(
        DVec3 velocity,
        DVec3 normal,
        f32& normalSpeed,
        DVec3& tangential
    ) noexcept;

    // 0 head-on, 1 grazing
    f32 tangentialShare(f32 normalSpeed, f32 tangentialSpeed) noexcept;

    void splitGroups(
        const PaintSplashProfile& profile,
        i32 count,
        f32 tangentialShare,
        i32& forward,
        i32& side,
        i32& back
    ) noexcept;

    // half a ball radius along the in-plane velocity, the droplet's radius
    // plus a centimetre off the surface
    DVec3 launchOffset(
        DVec3 normal,
        DVec3 velocity,
        f32 radius,
        f32 ballRadius
    ) noexcept;

    // largest first; none below minNormalSpeed
    void generateDroplets(
        const PaintSplashProfile& profile,
        const PaintSplashInput& input,
        std::vector<PaintDroplet>& droplets
    );

    // shrinks the radii together to the volume cap; the scale applied
    f32 capVolume(std::span<f32> radii, f32 ballRadius, f32 volumeFraction);

    // the largest maxScoreDroplets back on the contact's plane, drag
    // ignored; none on a wall or a ceiling. gravityZ is cm/s^2, down < 0
    void phantomLandings(
        const PaintSplashProfile& profile,
        const PaintSplashInput& input,
        f32 gravityZ,
        std::vector<PaintPhantomLanding>& landings
    );
}
