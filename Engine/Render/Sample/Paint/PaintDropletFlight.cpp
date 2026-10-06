#include "PaintDropletFlight.hpp"

#include <algorithm>
#include <cmath>

#include "MintRandom.hpp"

namespace Crowy
{
    bool PaintSplashFlight::IsAlive() const noexcept {
        return std::ranges::any_of(droplets, &PaintFlyingDroplet::alive);
    }

    PaintSplashFlight launchSplash(
        const PaintSplashProfile& profile,
        const PaintSplashInput& input,
        f32 splatRadius,
        u8 paintId,
        PaintLockGens lockGens,
        i64 onlySurface
    ) {
        std::vector<PaintDroplet> droplets;
        generateDroplets(profile, input, droplets);

        PaintSplashFlight flight{
            .contact = input.impactPoint,
            .normal = input.impactNormal,
            .markClearance = splatRadius * profile.markClearanceScale,
            .splashSeed = splashSeed(input.seed),
            .paintId = paintId,
            .lockGens = lockGens,
            .onlySurface = onlySurface
        };
        const auto origin = input.impactPoint + input.impactNormal;
        for(usize i = 0; i < droplets.size(); ++i) {
            flight.droplets.push_back(
                PaintFlyingDroplet{
                    .position = origin,
                    .velocity = droplets[i].velocity,
                    .radius = droplets[i].radius,
                    .launchSpeed = static_cast<f32>(size(droplets[i].velocity)),
                    .group = droplets[i].group,
                    .mayMark = static_cast<i32>(i) < profile.maxMarkDroplets
                }
            );
        }

        return flight;
    }

    void stepFlight(
        PaintSplashFlight& flight,
        const PaintSplashProfile& profile,
        const PaintSceneQuery& scene,
        std::vector<PaintDropletLanding>& landings
    ) {
        constexpr auto Dt = PaintFlightSubstep;
        const DVec3 gravity{0.0, 0.0, PaintGravityZ * profile.gravityScale};
        const auto k = static_cast<f64>(profile.drag);
        const auto decay = std::exp(-k * Dt);
        const auto maxPath = PaintFlightPathScale * profile.maxTravel;

        for(auto& droplet: flight.droplets) {
            if(!droplet.alive)
                continue;

            // v(t) = g/k + (v0 - g/k) e^(-kt), and its integral
            DVec3 position;
            DVec3 velocity;
            if(k > 1e-6) {
                const auto terminal = gravity / k;
                velocity = terminal + (droplet.velocity - terminal) * decay;
                position = droplet.position + terminal * Dt +
                           (droplet.velocity - terminal) * ((1.0 - decay) / k);
            } else {
                velocity = droplet.velocity + gravity * Dt;
                position = droplet.position + droplet.velocity * Dt +
                           gravity * (0.5 * Dt * Dt);
            }

            const auto path = position - droplet.position;
            const auto length = size(path);
            const auto hit = scene.Raycast(
                droplet.position,
                path,
                length,
                flight.onlySurface
            );
            if(hit) {
                droplet.position = hit->impactPoint;
                droplet.alive = false;
                if(droplet.mayMark) {
                    landings.push_back(
                        PaintDropletLanding{
                            .point = hit->impactPoint,
                            .normal = hit->impactNormal,
                            .launchSpeed = droplet.launchSpeed,
                            .surface = hit->surface
                        }
                    );
                }
                continue;
            }

            droplet.position = position;
            droplet.velocity = velocity;
            droplet.traveled += length;
            droplet.age += Dt;
            if(droplet.age > profile.maxLifetime || droplet.traveled > maxPath)
                droplet.alive = false;
        }
    }

    std::optional<PaintSplat> landingMark(
        PaintSplashFlight& flight,
        const PaintSplashProfile& profile,
        const PaintDropletLanding& landing
    ) {
        const auto clearance = static_cast<f64>(flight.markClearance);
        if(sizeSquared(landing.point - flight.contact) < clearance * clearance)
            return std::nullopt;
        if(flight.marksDrawn >= profile.maxMarkDroplets)
            return std::nullopt;

        // head-on at the launch speed: round, sized like the phantom the
        // score claimed for it
        ++flight.marksDrawn;
        const auto seed = static_cast<i32>(hashCombineFast(
            static_cast<u32>(flight.splashSeed),
            static_cast<u32>(flight.marksDrawn)
        ));
        auto splat = profile.dropletBrush.BuildSplat(
            PaintHit{
                .impactPoint = landing.point,
                .impactNormal = landing.normal,
                .surface = landing.surface
            },
            -landing.normal * landing.launchSpeed,
            flight.paintId,
            profile.dropletSplatVolume,
            profile.dropletHeightAdd,
            seed
        );
        splat.lockGens = flight.lockGens;
        splat.drawOnly = true;

        return splat;
    }
}
