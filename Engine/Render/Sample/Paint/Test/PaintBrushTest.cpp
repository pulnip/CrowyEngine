#include <cmath>

#include <gtest/gtest.h>

#include "MintRandom.hpp"
#include "PaintBrushProfile.hpp"
#include "PaintSurface.hpp"
#include "PaintWorld.hpp"

using namespace Crowy;

namespace
{
    PaintHit floorHit() {
        return PaintHit{
            .impactPoint = {0.0, 0.0, 0.0},
            .impactNormal = {0.0, 0.0, 1.0}
        };
    }

    // a ball travelling along +X and down at `thetaDeg` from the normal
    DVec3 incoming(f64 thetaDeg, f64 speed) {
        const auto theta = thetaDeg * 3.14159265358979323846 / 180.0;

        return DVec3{std::sin(theta), 0.0, -std::cos(theta)} * speed;
    }
}

TEST(MintRandom, StreamMutatesBeforeEachDraw) {
    // seed 0 mutates to 907633515 first; its top 23 bits are the mantissa
    RandomStream stream(0);
    const auto first = stream.FRand();
    EXPECT_EQ(stream.Seed(), 907633515u);
    EXPECT_FLOAT_EQ(first, static_cast<f32>(907633515u >> 9) / 8388608.0f);
    // every draw is in [0, 1) and the same seed repeats the sequence
    RandomStream again(0);
    EXPECT_EQ(again.FRand(), first);
    for(i32 i = 0; i < 1000; ++i) {
        const auto x = stream.FRand();
        EXPECT_GE(x, 0.0f);
        EXPECT_LT(x, 1.0f);
    }
    RandomStream ranged(42);
    const auto y = ranged.FRandRange(10.0, 20.0);
    EXPECT_GE(y, 10.0);
    EXPECT_LT(y, 20.0);
}

TEST(MintRandom, HashCombineAndRound) {
    EXPECT_EQ(hashCombineFast(0u, 0u), 0x9e3779b9u);
    EXPECT_EQ(hashCombineFast(1u, 0u), 1u ^ (0x9e3779b9u + 64u + 0u));
    EXPECT_EQ(roundToInt(8.5f), 9);
    EXPECT_EQ(roundToInt(-0.5f), 0);
    EXPECT_EQ(roundToInt(2.49999f), 2);
}

TEST(PaintBrush, RadiusGrowsWithVolumeAndSpeed) {
    const auto brush = PaintBrushProfile::Default();
    // the sample map's click: 25 sqrt(1) + 0.005 * 3000
    EXPECT_FLOAT_EQ(brush.ComputeRadius(1.0f, 3000.0f), 40.0f);
    // four times the volume doubles the radius
    EXPECT_FLOAT_EQ(brush.ComputeRadius(4.0f, 0.0f), 50.0f);
    EXPECT_FLOAT_EQ(brush.ComputeRadius(100.0f, 0.0f), 120.0f);
    EXPECT_NEAR(
        PaintBrushProfile::MopT().ComputeRadius(2.4f, 4800.0f),
        92.95f,
        0.01f
    );
}

TEST(PaintBrush, HeadOnIsRoundAndSeedSpun) {
    const auto brush = PaintBrushProfile::Default();
    const auto a =
        brush.BuildSplat(floorHit(), incoming(0.0, 3000.0), 0, 1.0f, 0.35f, 7);
    const auto b =
        brush.BuildSplat(floorHit(), incoming(0.0, 3000.0), 0, 1.0f, 0.35f, 7);
    const auto c =
        brush.BuildSplat(floorHit(), incoming(0.0, 3000.0), 0, 1.0f, 0.35f, 8);

    EXPECT_FLOAT_EQ(a.stretch, 1.0f);
    EXPECT_FLOAT_EQ(a.radius, 40.0f);
    EXPECT_EQ(a.location, (DVec3{0.0, 0.0, 0.0}));
    EXPECT_FLOAT_EQ(a.impactU, 0.0f);
    // the axis lies in the surface, and the seed alone turns it
    EXPECT_NEAR(dot(a.axisU, a.normal), 0.0, 1e-9);
    EXPECT_NEAR(size(a.axisU), 1.0, 1e-9);
    EXPECT_EQ(a.axisU, b.axisU);
    EXPECT_GT(size(a.axisU - c.axisU), 1e-3);
}

TEST(PaintBrush, GrazingStretchesAndSlidesAhead) {
    const auto brush = PaintBrushProfile::Default();
    const auto splat =
        brush.BuildSplat(floorHit(), incoming(60.0, 3000.0), 1, 1.0f, 0.35f, 3);

    EXPECT_NEAR(splat.stretch, 2.0f, 1e-4f);
    // aligned with the travel, the centre 20 cm ahead
    EXPECT_NEAR(splat.axisU.x, 1.0, 1e-9);
    EXPECT_NEAR(splat.location.x, 20.0, 1e-3);
    EXPECT_NEAR(splat.impactU, -0.25f, 1e-5f);
    EXPECT_EQ(splat.paintId, 1);
    EXPECT_EQ(splat.seed, 3);
}

TEST(PaintBrush, SeedKeepsSixteenBits) {
    const auto splat = PaintBrushProfile::Default().BuildSplat(
        floorHit(),
        incoming(0.0, 3000.0),
        0,
        1.0f,
        0.35f,
        0x12345
    );
    EXPECT_EQ(splat.seed, 0x2345);
}

TEST(PaintSurface, FloorFollowsWorldUpOnlyOnAWideFace) {
    const auto cube = makePaintCube(100.0);
    // a wall: 800 x 60 x 300, nothing ticked; its top is 800 x 60
    PaintStageObject wall{
        .transform = MintTransform{.scale = {8.0, 0.6, 3.0}},
        .directions = 0
    };
    EXPECT_EQ(
        PaintSurface(wall, cube).EnabledDirections(),
        paintDirectionBit(PaintFaceDirection::Up)
    );
    // a post, 40 x 40 on top: under the 2500 cm2 footprint
    PaintStageObject post{
        .transform = MintTransform{.scale = {0.4, 0.4, 3.0}},
        .directions = 0
    };
    EXPECT_EQ(PaintSurface(post, cube).EnabledDirections(), 0);
    // rolled onto its side, the face toward the sky is local -Y
    PaintStageObject rolled{
        .transform =
            MintTransform{.rotation = DQuat::FromRotator(0.0, 0.0, 90.0)},
        .directions = 0
    };
    const auto mask = PaintSurface(rolled, cube).EnabledDirections();
    EXPECT_EQ(mask, paintDirectionBit(PaintFaceDirection::Left));
}

TEST(PaintSurface, KeptDirectionsDecideTransience) {
    const auto cube = makePaintCube(100.0);
    PaintStageObject crate{
        .transform = MintTransform{.scale = {3.0, 3.0, 3.0}}
    };
    const PaintSurface surface(crate, cube);
    EXPECT_TRUE(surface.IsWorldNormalPersistent({0.0, 0.0, 1.0}));
    EXPECT_FALSE(surface.IsWorldNormalPersistent({1.0, 0.0, 0.0}));
    EXPECT_FALSE(surface.IsWorldNormalPersistent({0.0, 0.0, -1.0}));
}

TEST(PaintSurface, StampRectsStayInTheirIslands) {
    const auto cube = makePaintCube(100.0);
    PaintStageObject crate{
        .transform =
            MintTransform{
                .translation = {0.0, 0.0, 150.0},
                .scale = {3.0, 3.0, 3.0}
            },
        .directions = static_cast<u8>(
            paintDirectionBit(PaintFaceDirection::Up) |
            paintDirectionBit(PaintFaceDirection::Front)
        )
    };
    const PaintSurface surface(crate, cube);
    // a splat on the top 20 cm from the +X edge reaches the front too
    const auto splat = PaintBrushProfile::Default().BuildSplat(
        PaintHit{
            .impactPoint = {130.0, 0.0, 300.0},
            .impactNormal = {0.0, 0.0, 1.0}
        },
        incoming(0.0, 3000.0),
        0,
        1.0f,
        0.35f,
        1
    );
    const auto stamp = surface.ComputeLocalStamp(splat);
    EXPECT_NEAR(stamp.center.z, 150.0, 1e-9);
    std::vector<IntRect> rects;
    surface.BuildStampRects(stamp, rects);
    ASSERT_EQ(rects.size(), 2u);
    for(const auto& rect: rects) {
        bool inside = false;
        for(const auto& island: surface.Layout().islands) {
            inside |= rect.min.x >= island.rect.min.x &&
                      rect.min.y >= island.rect.min.y &&
                      rect.max.x <= island.rect.max.x &&
                      rect.max.y <= island.rect.max.y;
        }
        EXPECT_TRUE(inside);
    }
}

TEST(PaintSceneQuery, RaysDownADiagonalHitTheTop) {
    PaintWorld world;
    world.SetStage(PaintStageKind::Grid);
    // a tile's top is two triangles sharing a diagonal; a ray down that
    // edge must not slip between them to the bottom face
    const auto tile = static_cast<i64>(paintGridTileIndex(2, 0));
    i32 missed = 0;
    for(i32 i = 0; i < 400; ++i) {
        const auto t = -150.0 + 0.75 * i;
        for(const auto sign: {1.0, -1.0}) {
            const DVec3 target{900.0 + t, 300.0 + sign * t, 0.0};
            const auto theta = 0.1 + 0.003 * i;
            const auto origin =
                target -
                DVec3{std::sin(theta), 0.0, -std::cos(theta)} * (400.0 + i);
            const auto hit =
                world.Scene().Raycast(origin, target - origin, 2000.0, tile);
            if(!hit || hit->impactNormal.z != 1.0)
                ++missed;
        }
    }
    EXPECT_EQ(missed, 0);
}

TEST(PaintWorld, ReLayoutDropsTheSurfacesQueuedDraws) {
    PaintWorld world;
    const auto hit = world.Fire(
        PaintShot{
            .origin = {0.0, 0.0, 1000.0},
            .velocity = {0.0, 0.0, -3000.0},
            .seed = 5
        }
    );
    ASSERT_TRUE(hit.has_value());
    const auto floor = static_cast<usize>(hit->surface);
    world.SetDirections(
        floor,
        static_cast<u8>(
            paintDirectionBit(PaintFaceDirection::Up) |
            paintDirectionBit(PaintFaceDirection::Front)
        ),
        true
    );
    for(const auto& draw: world.TakeDraws())
        EXPECT_NE(draw.surface, floor);
}

TEST(PaintWorld, LaunchedBallFiresAfterItsLead) {
    PaintWorld world;
    ASSERT_TRUE(world.Launch(
        PaintShot{
            .origin = {0.0, 0.0, 1000.0},
            .velocity = {0.0, 0.0, -3000.0},
            .seed = 5
        },
        0.25
    ));
    // 0.25 s is 60 substeps
    world.Step(59);
    EXPECT_TRUE(world.Log().empty());
    ASSERT_EQ(world.Balls().size(), 1u);
    EXPECT_NEAR(world.Balls()[0].Position().z, 3000.0 / 240.0, 1e-6);
    world.Step(1);
    EXPECT_EQ(world.Log().size(), 1u);
    EXPECT_TRUE(world.Balls().empty());
}

TEST(PaintWorld, AShorterLeadFiresFirst) {
    PaintWorld world;
    const auto shot = [](i32 seed) {
        return PaintShot{
            .origin = {0.0, 0.0, 1000.0},
            .velocity = {0.0, 0.0, -3000.0},
            .seed = seed
        };
    };
    ASSERT_TRUE(world.Launch(shot(1), 0.5));
    ASSERT_TRUE(world.Launch(shot(2), 0.1));
    world.Step(24);
    ASSERT_EQ(world.Log().size(), 1u);
    EXPECT_EQ(world.Log()[0].seed, 2);
    EXPECT_EQ(world.Balls().size(), 1u);
}

TEST(PaintWorld, KeptAndTransientShots) {
    PaintWorld world;
    // straight down onto the floor's middle: kept, drawn
    const auto floor = world.Fire(
        PaintShot{
            .origin = {0.0, 0.0, 1000.0},
            .velocity = {0.0, 0.0, -3000.0},
            .seed = 5
        }
    );
    ASSERT_TRUE(floor.has_value());
    EXPECT_EQ(
        world.Surfaces()[static_cast<usize>(floor->surface)].Object().name,
        "floor"
    );
    ASSERT_EQ(world.Log().size(), 1u);
    EXPECT_FALSE(world.Log()[0].transient);
    const auto draws = world.TakeDraws();
    ASSERT_FALSE(draws.empty());
    EXPECT_TRUE(world.TakeDraws().empty());

    // into the crate's side, which it does not keep: an effect only
    const auto side = world.Fire(
        PaintShot{
            .origin = {-1000.0, -300.0, 150.0},
            .velocity = {3000.0, 0.0, 0.0},
            .seed = 6
        }
    );
    ASSERT_TRUE(side.has_value());
    EXPECT_EQ(
        world.Surfaces()[static_cast<usize>(side->surface)].Object().name,
        "crate"
    );
    EXPECT_TRUE(world.Log().back().transient);
    EXPECT_TRUE(world.TakeDraws().empty());

    world.Reset();
    EXPECT_TRUE(world.Log().empty());
}
