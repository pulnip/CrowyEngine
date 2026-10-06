#include <cmath>
#include <vector>

#include <gtest/gtest.h>

#include "PaintDropletFlight.hpp"
#include "PaintSplash.hpp"
#include "PaintWorld.hpp"

using namespace Crowy;

// MintChoco's Tests/PaintSplashTest.cpp, and the flight that stands in for
// NS_PaintSplash
namespace
{
    // PaintSplashTestActor's contact: a ball skimming forward into the floor
    PaintSplashInput floorContact(i32 seed = 7) {
        return PaintSplashInput{
            .impactPoint = {0.0, 0.0, 0.0},
            .impactNormal = {0.0, 0.0, 1.0},
            .incidentVelocity = {1500.0, 0.0, -2500.0},
            .ballRadius = 6.0f,
            .seed = seed
        };
    }

    i32 countGroup(
        const std::vector<PaintDroplet>& droplets,
        PaintDropletGroup group
    ) {
        i32 count = 0;
        for(const auto& d: droplets)
            count += d.group == group ? 1 : 0;

        return count;
    }
}

TEST(PaintSplash, ContactSplitsNineFiveTwo) {
    const auto profile = PaintSplashProfile::Paintball();
    f32 normalSpeed = 0.0f;
    DVec3 tangential;
    splitVelocity(
        {1500.0, 0.0, -2500.0},
        {0.0, 0.0, 1.0},
        normalSpeed,
        tangential
    );
    EXPECT_EQ(normalSpeed, 2500.0f);
    EXPECT_EQ(tangential, (DVec3{1500.0, 0.0, 0.0}));

    // 16 x lerp(.4, .75, .375) = 8.5 sits on the rounding edge and goes up
    const auto share = tangentialShare(normalSpeed, 1500.0f);
    EXPECT_EQ(share, 0.375f);
    i32 forward = 0;
    i32 side = 0;
    i32 back = 0;
    splitGroups(profile, 16, share, forward, side, back);
    EXPECT_EQ(forward, 9);
    EXPECT_EQ(side, 5);
    EXPECT_EQ(back, 2);

    std::vector<PaintDroplet> droplets;
    generateDroplets(profile, floorContact(), droplets);
    EXPECT_EQ(countGroup(droplets, PaintDropletGroup::Forward), 9);
    EXPECT_EQ(countGroup(droplets, PaintDropletGroup::Side), 5);
    EXPECT_EQ(countGroup(droplets, PaintDropletGroup::Back), 2);
}

TEST(PaintSplash, Determinism) {
    const auto profile = PaintSplashProfile::Paintball();
    std::vector<PaintDroplet> a;
    std::vector<PaintDroplet> b;
    generateDroplets(profile, floorContact(), a);
    generateDroplets(profile, floorContact(), b);
    ASSERT_EQ(a.size(), static_cast<usize>(profile.dropletCount));
    for(usize i = 0; i < a.size(); ++i) {
        EXPECT_EQ(a[i].velocity, b[i].velocity);
        EXPECT_EQ(a[i].radius, b[i].radius);
        // leaves the surface, under the speed cap, largest first
        EXPECT_GT(a[i].velocity.z, 0.0);
        EXPECT_LE(size(a[i].velocity), profile.maxDropletSpeed + 1e-3);
        if(i > 0)
            EXPECT_GE(a[i - 1].radius, a[i].radius);
        const auto offset =
            launchOffset({0.0, 0.0, 1.0}, a[i].velocity, a[i].radius, 6.0f);
        EXPECT_EQ(a[i].position, offset);
    }

    std::vector<PaintDroplet> other;
    generateDroplets(profile, floorContact(8), other);
    EXPECT_NE(other[1].velocity, a[1].velocity);
}

TEST(PaintSplash, GrazingThrowsMoreForwardAndSlowDoesNotSplash) {
    const auto profile = PaintSplashProfile::Paintball();
    const auto forwardOf = [&](DVec3 velocity) {
        auto input = floorContact();
        input.incidentVelocity = velocity;
        std::vector<PaintDroplet> droplets;
        generateDroplets(profile, input, droplets);
        return countGroup(droplets, PaintDropletGroup::Forward);
    };
    EXPECT_GT(forwardOf({2500.0, 0.0, -600.0}), forwardOf({0.0, 0.0, -2500.0}));

    auto slow = floorContact();
    slow.incidentVelocity = {100.0, 0.0, -300.0};
    std::vector<PaintDroplet> none;
    generateDroplets(profile, slow, none);
    EXPECT_TRUE(none.empty());
}

TEST(PaintSplash, WallFrameFollowsTheSurface) {
    const auto profile = PaintSplashProfile::Paintball();
    const DVec3 normal{1.0, 0.0, 0.0};
    const DVec3 velocity{-2000.0, 300.0, -400.0};
    const auto travel = getSafeNormal(DVec3{0.0, 300.0, -400.0});
    i32 forward = 0;
    i32 side = 0;
    i32 back = 0;
    splitGroups(
        profile,
        profile.dropletCount,
        tangentialShare(2000.0f, 500.0f),
        forward,
        side,
        back
    );
    ASSERT_GT(back, 0);
    for(i32 seed = 1; seed <= 16; ++seed) {
        auto input = floorContact(seed);
        input.impactNormal = normal;
        input.incidentVelocity = velocity;
        std::vector<PaintDroplet> droplets;
        generateDroplets(profile, input, droplets);
        for(const auto& d: droplets) {
            EXPECT_GT(dot(d.velocity, normal), 0.0);
            if(d.group == PaintDropletGroup::Forward)
                EXPECT_GT(dot(d.velocity, travel), 0.0);
            if(d.group == PaintDropletGroup::Back)
                EXPECT_LT(dot(d.velocity, travel), 0.0);
        }
        EXPECT_EQ(countGroup(droplets, PaintDropletGroup::Forward), forward);
        EXPECT_EQ(countGroup(droplets, PaintDropletGroup::Back), back);
    }
}

TEST(PaintSplash, GroupsSumAndGrowForward) {
    const auto profile = PaintSplashProfile::Paintball();
    for(i32 count = 1; count <= PaintMaxDroplets; ++count) {
        i32 lastForward = 0;
        for(i32 step = 0; step <= 20; ++step) {
            i32 forward = 0;
            i32 side = 0;
            i32 back = 0;
            splitGroups(
                profile,
                count,
                static_cast<f32>(step) / 20.0f,
                forward,
                side,
                back
            );
            EXPECT_EQ(forward + side + back, count);
            EXPECT_GE(forward, lastForward);
            lastForward = forward;
        }
    }
    EXPECT_EQ(tangentialShare(2500.0f, 0.0f), 0.0f);
    EXPECT_NEAR(tangentialShare(1000.0f, 1000.0f), 0.5f, 1e-4f);
}

TEST(PaintSplash, VolumeCap) {
    std::vector<f32> radii{6.0f, 6.0f, 6.0f, 6.0f};
    EXPECT_NEAR(capVolume(radii, 6.0f, 0.5f), 0.5f, 1e-4f);
    EXPECT_NEAR(radii[2], 3.0f, 1e-4f);
    std::vector<f32> small{1.0f, 1.0f};
    EXPECT_EQ(capVolume(small, 6.0f, 0.5f), 1.0f);

    auto profile = PaintSplashProfile::Paintball();
    profile.dropletRadiusScaleMin = 2.0f;
    profile.dropletRadiusScaleMax = 2.0f;
    profile.volumeFraction = 0.5f;
    std::vector<PaintDroplet> droplets;
    generateDroplets(profile, floorContact(), droplets);
    f64 total = 0.0;
    for(const auto& d: droplets)
        total += std::pow(static_cast<f64>(d.radius), 3.0);
    EXPECT_LE(total, 216.0 * 0.5 * (1.0 + 1e-3));
    EXPECT_LE(droplets.size(), static_cast<usize>(PaintMaxDroplets));
}

TEST(PaintSplash, PhantomLandings) {
    auto profile = PaintSplashProfile::Paintball();
    std::vector<PaintPhantomLanding> landings;
    phantomLandings(profile, floorContact(), PaintGravityZ, landings);
    ASSERT_FALSE(landings.empty());
    EXPECT_LE(landings.size(), static_cast<usize>(profile.maxScoreDroplets));

    // the first is the largest droplet's drag-free parabola back to z = 0
    std::vector<PaintDroplet> droplets;
    generateDroplets(profile, floorContact(), droplets);
    const auto& largest = droplets[0];
    const auto g = static_cast<f64>(PaintGravityZ);
    const auto time =
        (-largest.velocity.z - std::sqrt(
                                   largest.velocity.z * largest.velocity.z -
                                   2.0 * g * largest.position.z
                               )) /
        g;
    const auto expected = largest.position + largest.velocity * time +
                          DVec3{0.0, 0.0, 0.5 * g * time * time};
    EXPECT_NEAR(landings[0].point.x, expected.x, 1e-2);
    EXPECT_NEAR(landings[0].point.y, expected.y, 1e-2);
    for(const auto& landing: landings) {
        EXPECT_NEAR(landing.point.z, 0.0, 1e-6);
        EXPECT_LE(size(landing.point), profile.maxTravel);
        EXPECT_GT(landing.speed, 0.0f);
    }

    profile.maxScoreDroplets = 0;
    phantomLandings(profile, floorContact(), PaintGravityZ, landings);
    EXPECT_TRUE(landings.empty());
    profile.maxScoreDroplets = 4;

    // gravity never brings a droplet back to a wall or a ceiling
    auto wall = floorContact();
    wall.impactNormal = {1.0, 0.0, 0.0};
    wall.incidentVelocity = {-2000.0, 0.0, -400.0};
    phantomLandings(profile, wall, PaintGravityZ, landings);
    EXPECT_TRUE(landings.empty());
    auto ceiling = floorContact();
    ceiling.impactNormal = {0.0, 0.0, -1.0};
    ceiling.incidentVelocity = {500.0, 0.0, 2500.0};
    phantomLandings(profile, ceiling, PaintGravityZ, landings);
    EXPECT_TRUE(landings.empty());
}

TEST(PaintDropletFlight, DragStepMatchesTheClosedForm) {
    const auto profile = PaintSplashProfile::Paintball();
    const PaintSceneQuery empty;
    PaintSplashFlight flight;
    flight.droplets.push_back(
        PaintFlyingDroplet{
            .position = {0.0, 0.0, 1.0},
            .velocity = {300.0, 0.0, 500.0},
            .mayMark = true
        }
    );
    std::vector<PaintDropletLanding> landings;
    constexpr i32 Steps = 120;
    for(i32 i = 0; i < Steps; ++i)
        stepFlight(flight, profile, empty, landings);

    const auto t = Steps * PaintFlightSubstep;
    const auto k = static_cast<f64>(profile.drag);
    const DVec3 terminal{0.0, 0.0, PaintGravityZ / k};
    const DVec3 v0{300.0, 0.0, 500.0};
    const auto x = DVec3{0.0, 0.0, 1.0} + terminal * t +
                   (v0 - terminal) * ((1.0 - std::exp(-k * t)) / k);
    const auto& droplet = flight.droplets[0];
    EXPECT_NEAR(droplet.position.x, x.x, 1e-6);
    EXPECT_NEAR(droplet.position.z, x.z, 1e-6);
    EXPECT_TRUE(landings.empty());
}

TEST(PaintDropletFlight, KilledByLifetimeAndPath) {
    auto profile = PaintSplashProfile::Paintball();
    const PaintSceneQuery empty;
    std::vector<PaintDropletLanding> landings;

    // no gravity, no drag: 100 cm/s never walks 600 cm, so 1.8 s ends it
    profile.gravityScale = 0.0f;
    profile.drag = 0.0f;
    PaintSplashFlight slow;
    slow.droplets.push_back(
        PaintFlyingDroplet{.velocity = {100.0, 0.0, 0.0}, .mayMark = true}
    );
    i32 steps = 0;
    while(slow.IsAlive()) {
        stepFlight(slow, profile, empty, landings);
        ++steps;
    }
    EXPECT_NEAR(steps, 433, 1);

    // 900 cm/s outruns 1.5 x 400 cm of path in 2/3 s
    PaintSplashFlight flat;
    flat.droplets.push_back(
        PaintFlyingDroplet{.velocity = {900.0, 0.0, 0.0}, .mayMark = true}
    );
    steps = 0;
    while(flat.IsAlive()) {
        stepFlight(flat, profile, empty, landings);
        ++steps;
    }
    EXPECT_NEAR(steps, 161, 1);
    EXPECT_TRUE(landings.empty());
}

TEST(PaintDropletFlight, ClearanceAndMarkCap) {
    const auto profile = PaintSplashProfile::Paintball();
    PaintSplashFlight flight{.markClearance = 50.0f, .splashSeed = 3};
    const auto at = [](f64 x) {
        return PaintDropletLanding{
            .point = {x, 0.0, 0.0},
            .launchSpeed = 400.0f,
            .surface = 0
        };
    };
    EXPECT_FALSE(landingMark(flight, profile, at(30.0)).has_value());
    EXPECT_EQ(flight.marksDrawn, 0);

    for(i32 i = 0; i < profile.maxMarkDroplets; ++i) {
        const auto mark = landingMark(flight, profile, at(100.0 + i));
        ASSERT_TRUE(mark.has_value());
        EXPECT_TRUE(mark->drawOnly);
        EXPECT_EQ(mark->stretch, 1.0f);
        EXPECT_FLOAT_EQ(
            mark->radius,
            profile.dropletBrush
                .ComputeRadius(profile.dropletSplatVolume, 400.0f)
        );
    }
    EXPECT_FALSE(landingMark(flight, profile, at(300.0)).has_value());
    EXPECT_EQ(flight.marksDrawn, profile.maxMarkDroplets);
}

TEST(PaintWorld, SplashScoresPhantomsAndDrawsMarks) {
    PaintWorld plain;
    PaintWorld splashed;
    // as MintChoco's test: a droplet's mark one score cell wide, so each
    // phantom claims the cell it lands in
    auto profile = PaintSplashProfile::Paintball();
    profile.dropletBrush.baseRadius = PaintScoreCellSize;
    profile.dropletBrush.radiusPerSpeed = 0.0f;
    profile.dropletBrush.maxRadius = PaintScoreCellSize;
    profile.dropletSplatVolume = 1.0f;
    splashed.SetSplash(profile);
    const PaintShot shot{
        .origin = {300.0 - 1500.0 * 0.4, 300.0, 2500.0 * 0.4},
        .velocity = {1500.0, 0.0, -2500.0},
        .seed = 11,
        .splash = true
    };
    auto noSplash = shot;
    noSplash.splash = false;
    plain.Fire(noSplash);
    splashed.Fire(shot);

    // the score: phantoms add cells right away, on every machine alike
    ASSERT_FALSE(splashed.Phantoms().empty());
    EXPECT_GT(
        splashed.Coverage().areaByPaintId[0],
        plain.Coverage().areaByPaintId[0]
    );
    const auto scored = splashed.Coverage().areaByPaintId[0];

    // the picture: droplets fly, land and draw, and claim no cell
    ASSERT_EQ(splashed.Flights().size(), 1u);
    (void)splashed.TakeDraws();
    for(i32 i = 0; i < 2 * 240 && !splashed.Flights().empty(); ++i)
        splashed.Step(1);
    EXPECT_TRUE(splashed.Flights().empty());
    EXPECT_FALSE(splashed.Marks().empty());
    EXPECT_LE(
        splashed.Marks().size(),
        static_cast<usize>(splashed.Splash().maxMarkDroplets)
    );
    EXPECT_FALSE(splashed.TakeDraws().empty());
    EXPECT_EQ(splashed.Coverage().areaByPaintId[0], scored);
    EXPECT_EQ(splashed.Log().size(), 1u);
}
