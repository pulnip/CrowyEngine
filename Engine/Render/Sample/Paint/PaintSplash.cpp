#include "PaintSplash.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <functional>
#include <numbers>

#include "MintRandom.hpp"

namespace Crowy
{
    namespace
    {
        // FMath::DegreesToRadians on a float
        f32 radians(f32 degrees) noexcept {
            return degrees * (std::numbers::pi_v<f32> / 180.0f);
        }

        f32 lerp(f32 a, f32 b, f32 t) noexcept {
            return a + t * (b - a);
        }

        DVec3 normalOrUp(DVec3 normal) noexcept {
            const auto n = getSafeNormal(normal);
            return n == DVec3{} ? DVec3{0.0, 0.0, 1.0} : n;
        }

        // a head-on hit has no travel to follow: an in-plane axis spun by
        // the seed, so the pattern still varies
        DVec3 seededTangent(DVec3 normal, RandomStream& stream) {
            const auto reference = std::abs(normal.z) < 0.9
                                       ? DVec3{0.0, 0.0, 1.0}
                                       : DVec3{1.0, 0.0, 0.0};
            const auto base = getSafeNormal(cross(normal, reference));

            return rotateAngleAxis(base, stream.FRand() * 360.0f, normal);
        }

        DVec3 clampSpeed(DVec3 velocity, f32 maxSpeed) noexcept {
            const auto speed = size(velocity);

            return speed > maxSpeed && speed > DoubleKindaSmallNumber
                       ? velocity * (maxSpeed / speed)
                       : velocity;
        }
    }

    PaintSplashProfile PaintSplashProfile::Paintball() noexcept {
        PaintSplashProfile profile;
        profile.dropletSplatVolume = 1.0f;

        return profile;
    }

    i32 splashSeed(i32 ballSeed) noexcept {
        return static_cast<i32>(
            hashCombineFast(static_cast<u32>(ballSeed), 0x53504C48u)
        );
    }

    void splitVelocity(
        DVec3 velocity,
        DVec3 normal,
        f32& normalSpeed,
        DVec3& tangential
    ) noexcept {
        const auto along = dot(velocity, normal);
        normalSpeed = static_cast<f32>(-along);
        tangential = velocity - normal * along;
    }

    f32 tangentialShare(f32 normalSpeed, f32 tangentialSpeed) noexcept {
        const auto tangential = std::max(tangentialSpeed, 0.0f);

        return tangential / std::max(
                                tangential + std::max(normalSpeed, 0.0f),
                                KindaSmallNumber
                            );
    }

    void splitGroups(
        const PaintSplashProfile& profile,
        i32 count,
        f32 tangentialShare,
        i32& forward,
        i32& side,
        i32& back
    ) noexcept {
        count = std::max(count, 0);
        const auto forwardShare = lerp(
            profile.forwardShareHeadOn,
            profile.forwardShareGrazing,
            std::clamp(tangentialShare, 0.0f, 1.0f)
        );
        forward = std::clamp(
            roundToInt(
                static_cast<f32>(count) * std::clamp(forwardShare, 0.0f, 1.0f)
            ),
            0,
            count
        );
        const auto rest = count - forward;
        back = std::clamp(
            roundToInt(
                static_cast<f32>(rest) *
                std::clamp(profile.backShareOfRest, 0.0f, 1.0f)
            ),
            0,
            rest
        );
        side = rest - back;
    }

    DVec3 launchOffset(
        DVec3 normal,
        DVec3 velocity,
        f32 radius,
        f32 ballRadius
    ) noexcept {
        const auto flat = velocity - normal * dot(velocity, normal);
        const auto direction =
            size(flat) > 1e-3 ? getSafeNormal(flat) : DVec3{};

        return direction * (0.5 * ballRadius) + normal * (radius + 1.0);
    }

    void generateDroplets(
        const PaintSplashProfile& profile,
        const PaintSplashInput& input,
        std::vector<PaintDroplet>& droplets
    ) {
        droplets.clear();

        const auto normal = normalOrUp(input.impactNormal);
        f32 normalSpeed = 0.0f;
        DVec3 tangential;
        splitVelocity(input.incidentVelocity, normal, normalSpeed, tangential);
        if(normalSpeed < profile.minNormalSpeed)
            return;

        RandomStream stream(splashSeed(input.seed));
        const auto tangentialSpeed = static_cast<f32>(size(tangential));
        const auto launchSpeed =
            normalSpeed + profile.tangentialLaunchShare * tangentialSpeed;
        const auto forward = tangentialSpeed > 1.0f
                                 ? tangential / tangentialSpeed
                                 : seededTangent(normal, stream);
        const auto side = getSafeNormal(cross(normal, forward));
        const auto ballRadius = std::max(input.ballRadius, 0.1f);

        const auto count =
            std::clamp(profile.dropletCount, 1, PaintMaxDroplets);
        i32 numForward = 0;
        i32 numSide = 0;
        i32 numBack = 0;
        splitGroups(
            profile,
            count,
            tangentialShare(normalSpeed, tangentialSpeed),
            numForward,
            numSide,
            numBack
        );

        struct Plan {
            PaintDropletGroup group;
            const PaintSplashGroup& settings;
            i32 count;
            f32 headingDeg;
        };
        const std::array<Plan, 3> plans{
            Plan{PaintDropletGroup::Forward, profile.forward, numForward, 0.0f},
            Plan{PaintDropletGroup::Side, profile.side, numSide, 90.0f},
            Plan{PaintDropletGroup::Back, profile.back, numBack, 180.0f},
        };
        const auto radiusMin = std::max(profile.dropletRadiusScaleMin, 0.01f);
        const auto radiusMax =
            std::max(profile.dropletRadiusScaleMax, radiusMin);
        const auto radiusBias = std::max(profile.dropletRadiusBias, 0.01f);
        for(const auto& plan: plans) {
            for(i32 index = 0; index < plan.count; ++index) {
                // stratified across the fan, the side group alternating; the
                // draws keep a fixed order so one knob moves nothing else
                const auto fan = plan.settings.spreadDeg *
                                 ((static_cast<f32>(index) + stream.FRand()) /
                                      static_cast<f32>(plan.count) * 2.0f -
                                  1.0f);
                const auto heading =
                    plan.group == PaintDropletGroup::Side && index % 2 == 1
                        ? -plan.headingDeg
                        : plan.headingDeg;
                const auto azimuth = heading + fan;
                const auto elevation = static_cast<f32>(std::clamp(
                    stream.FRandRange(
                        plan.settings.elevationMinDeg,
                        plan.settings.elevationMaxDeg
                    ),
                    0.0,
                    89.0
                ));
                const auto speed = static_cast<f32>(
                    launchSpeed * stream.FRandRange(
                                      plan.settings.speedScaleMin,
                                      plan.settings.speedScaleMax
                                  )
                );
                const auto radiusScale =
                    radiusMin + (radiusMax - radiusMin) *
                                    std::pow(stream.FRand(), radiusBias);

                const auto inPlane = forward * std::cos(radians(azimuth)) +
                                     side * std::sin(radians(azimuth));
                const auto direction = normal * std::cos(radians(elevation)) +
                                       inPlane * std::sin(radians(elevation));
                const auto slide =
                    forward * (plan.settings.slideScale * tangentialSpeed);

                droplets.push_back(
                    PaintDroplet{
                        .velocity = clampSpeed(
                            direction * speed + slide,
                            profile.maxDropletSpeed
                        ),
                        .radius = radiusScale * ballRadius,
                        .group = plan.group
                    }
                );
            }
        }

        std::array<f32, PaintMaxDroplets> radii{};
        for(usize i = 0; i < droplets.size(); ++i)
            radii[i] = droplets[i].radius;
        const auto scale = capVolume(
            std::span(radii).first(droplets.size()),
            ballRadius,
            profile.volumeFraction
        );
        for(auto& droplet: droplets) {
            if(scale < 1.0f)
                droplet.radius *= scale;
            droplet.position = input.impactPoint + launchOffset(
                                                       normal,
                                                       droplet.velocity,
                                                       droplet.radius,
                                                       ballRadius
                                                   );
        }

        // largest first, so the mark and score caps take prefixes
        std::ranges::stable_sort(
            droplets,
            std::ranges::greater{},
            &PaintDroplet::radius
        );
    }

    f32 capVolume(std::span<f32> radii, f32 ballRadius, f32 volumeFraction) {
        // spheres share the 4/3 pi, so the cap is a ratio of cubes
        const auto cube = [](f64 x) {
            return x * x * x;
        };
        f64 total = 0.0;
        for(const auto radius: radii)
            total += cube(static_cast<f64>(std::max(radius, 0.0f)));
        const auto allowed =
            cube(static_cast<f64>(std::max(ballRadius, 0.0f))) *
            std::max(volumeFraction, 0.0f);
        if(total <= allowed || total <= SmallNumber)
            return 1.0f;

        const auto scale =
            static_cast<f32>(std::pow(allowed / total, 1.0 / 3.0));
        for(auto& radius: radii)
            radius *= scale;

        return scale;
    }

    void phantomLandings(
        const PaintSplashProfile& profile,
        const PaintSplashInput& input,
        f32 gravityZ,
        std::vector<PaintPhantomLanding>& landings
    ) {
        landings.clear();

        std::vector<PaintDroplet> droplets;
        generateDroplets(profile, input, droplets);
        if(droplets.empty())
            return;

        const auto normal = normalOrUp(input.impactNormal);
        const DVec3 gravity{0.0, 0.0, gravityZ * profile.gravityScale};
        // gravity's pull back toward the plane; none means the droplet never
        // returns to this surface, and the score does not guess where it falls
        const auto gravityAlong = dot(gravity, normal);
        if(gravityAlong >= -DoubleKindaSmallNumber)
            return;

        const auto count = std::min(
            static_cast<i32>(droplets.size()),
            std::max(profile.maxScoreDroplets, 0)
        );
        for(i32 i = 0; i < count; ++i) {
            const auto& droplet = droplets[static_cast<usize>(i)];
            // h(t) = h0 + v t + a t^2 / 2 with a < 0; the later root lands
            const auto height =
                dot(droplet.position - input.impactPoint, normal);
            const auto rise = dot(droplet.velocity, normal);
            const auto discriminant = rise * rise - 2.0 * gravityAlong * height;
            if(discriminant < 0.0)
                continue;
            const auto time = (-rise - std::sqrt(discriminant)) / gravityAlong;
            if(time <= 0.0 || time > profile.maxLifetime)
                continue;

            const auto point = droplet.position + droplet.velocity * time +
                               gravity * (0.5 * time * time);
            if(size(point - input.impactPoint) > profile.maxTravel)
                continue;
            landings.push_back(
                PaintPhantomLanding{
                    .point = point,
                    .speed = static_cast<f32>(
                        size(droplet.velocity + gravity * time)
                    )
                }
            );
        }
    }
}
