#include <gtest/gtest.h>

#include "BodyPrimitiveSync.hpp"
#include "LinearAlgebra.hpp"
#include "PhysicsRuntime.hpp"
#include "PhysicsWorld.hpp"
#include "RenderScene.hpp"

using namespace Crowy;

namespace
{
    // (1, 2, 3, 4) normalized: every component non-zero
    constexpr Vec4 Tilted{0.18257418f, 0.36514837f, 0.54772256f, 0.73029674f};
    constexpr Vec3 Placement{-3.5f, 0.5f, 1.0f};

    BodyBinding bindingOf(
        const PhysicsWorld& world,
        BodyHandle body,
        PrimitiveHandle primitive
    ) {
        return BodyBinding{
            .body = body,
            .primitive = primitive,
            .meshScale = unitMeshScaleOf(world.ShapeOf(body)),
        };
    }
}

TEST(BodyPrimitiveSync, WritesThePoseThroughThePlacement) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    const auto body = world.CreateBody(BodyDesc{
        .shape = BoxShape{{0.2f, 0.3f, 0.4f}},
        .pose = BodyPose{.position = {0.5f, 0.75f, -0.25f}, .rotation = Tilted},
        .motion = BodyMotion::Static,
    });
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(Placement);
    // off center, so bounds that ignored the binding's would show
    auto binding = bindingOf(world, body, primitive);
    binding.localBounds = AABB3D{
        .center = {0.0f, 0.5f, 0.0f},
        .halfScale = {0.5f, 0.25f, 0.5f},
    };
    sync.Bind(binding);

    sync.Sync(world, scene);
    const auto pose = world.PoseOf(body);
    const auto expected = translateMat(Placement) *
        modelMat(pose.position, pose.rotation, Vec3{0.4f, 0.6f, 0.8f});
    const auto& written = scene.Primitives().GetRef(primitive);
    for(usize column = 0; column < 4; ++column)
        EXPECT_EQ(written.localToWorld[column], expected[column]);
    EXPECT_EQ(
        static_cast<Vec3>(written.localToWorld[3]),
        Placement + pose.position
    );

    // the bounds come from the same matrix, so they agree to the bit
    const auto bounds = transformAABB3D(expected, binding.localBounds);
    EXPECT_EQ(written.worldBounds.center, bounds.center);
    EXPECT_EQ(written.worldBounds.halfScale, bounds.halfScale);
    EXPECT_EQ(world.BodyCount(), 1u);
    EXPECT_EQ(world.TickCount(), 0u);
}

TEST(BodyPrimitiveSync, QuarterTurnSwapsTheBoundsAxes) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    const auto body = world.CreateBody(BodyDesc{
        .shape = BoxShape{{1.0f, 0.5f, 0.25f}},
        .pose = BodyPose{
            .position = {2.0f, 1.0f, 3.0f},
            .rotation = {0.0f, 0.70710677f, 0.0f, 0.70710677f},
        },
        .motion = BodyMotion::Static,
    });
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(zeros());
    sync.Bind(bindingOf(world, body, primitive));

    sync.Sync(world, scene);
    const auto& bounds = scene.Primitives().GetRef(primitive).worldBounds;
    EXPECT_EQ(bounds.center, (Vec3{2.0f, 1.0f, 3.0f}));
    EXPECT_NEAR(bounds.halfScale.x, 0.25f, 1.0e-6f);
    EXPECT_NEAR(bounds.halfScale.y, 0.5f, 1.0e-6f);
    EXPECT_NEAR(bounds.halfScale.z, 1.0f, 1.0e-6f);
    EXPECT_EQ(world.BodyCount(), 1u);
    EXPECT_EQ(world.TickCount(), 0u);
}

TEST(BodyPrimitiveSync, LeavesUnboundPrimitivesAlone) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    const auto body = world.CreateBody(BodyDesc{.shape = SphereShape{0.15f}});
    const auto bound = scene.Primitives().Add(PrimitiveSnapshot{});
    const auto sentinel = PrimitiveSnapshot{
        .localToWorld = translateMat({9.0f, 9.0f, 9.0f}),
        .worldBounds = AABB3D{
            .center = {9.0f, 9.0f, 9.0f},
            .halfScale = {0.1f, 0.1f, 0.1f},
        },
    };
    const auto untouched = scene.Primitives().Add(sentinel);
    BodyPrimitiveSync sync(Placement);
    sync.Bind(bindingOf(world, body, bound));

    sync.Sync(world, scene);
    const auto& after = scene.Primitives().GetRef(untouched);
    for(usize column = 0; column < 4; ++column)
        EXPECT_EQ(after.localToWorld[column], sentinel.localToWorld[column]);
    EXPECT_EQ(after.worldBounds.center, sentinel.worldBounds.center);
    EXPECT_EQ(after.worldBounds.halfScale, sentinel.worldBounds.halfScale);
    EXPECT_EQ(scene.Primitives().Count(), 2u);
    EXPECT_EQ(world.BodyCount(), 1u);
    EXPECT_EQ(world.TickCount(), 0u);
}

TEST(BodyPrimitiveSync, SphereBoundsDoNotTurn) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    // an eighth of a turn about z, which would grow a box's bounds by 1.41
    const auto ball = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.15f},
        .pose = BodyPose{
            .position = {1.0f, 0.15f, 2.0f},
            .rotation = {0.0f, 0.0f, 0.38268343f, 0.92387953f},
        },
        .motion = BodyMotion::Static,
    });
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(Placement);
    sync.Bind(bindingOf(world, ball, primitive));

    sync.Sync(world, scene);
    const auto& bounds = scene.Primitives().GetRef(primitive).worldBounds;
    EXPECT_EQ(bounds.halfScale, 0.15f * ones());
    EXPECT_EQ(bounds.center, Placement + world.PoseOf(ball).position);
    EXPECT_EQ(world.BodyCount(), 1u);
    EXPECT_EQ(world.TickCount(), 0u);
}

TEST(BodyPrimitiveSync, FollowsAFallingBody) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    const auto ball = world.CreateBody(BodyDesc{
        .shape = SphereShape{0.15f},
        .pose = BodyPose{.position = {0.0f, 1.0f, 0.0f}},
    });
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(Placement);
    sync.Bind(bindingOf(world, ball, primitive));

    for(u32 tick = 0; tick < 30; ++tick)
        world.Step();
    sync.Sync(world, scene);
    const auto pose = world.PoseOf(ball);
    const auto& written = scene.Primitives().GetRef(primitive);
    EXPECT_EQ(
        static_cast<Vec3>(written.localToWorld[3]),
        Placement + pose.position
    );
    EXPECT_LT(pose.position.y, 0.0f);
    EXPECT_EQ(world.TickCount(), 30u);
    EXPECT_EQ(world.BodyCount(), 1u);
}

TEST(BodyPrimitiveSync, UnitMeshScaleFitsTheShape) {
    EXPECT_EQ(
        unitMeshScaleOf(BoxShape{{0.2f, 0.3f, 0.4f}}),
        (Vec3{0.4f, 0.6f, 0.8f})
    );
    EXPECT_EQ(unitMeshScaleOf(SphereShape{0.15f}), 0.3f * ones());
}

#if defined(_DEBUG) || !defined(NDEBUG)
TEST(BodyPrimitiveSyncDeathTest, OneWriterPerPrimitive) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            RenderScene scene;
            const auto body = world.CreateBody(BodyDesc{});
            const auto other = world.CreateBody(BodyDesc{});
            const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
            BodyPrimitiveSync sync(zeros());
            sync.Bind(bindingOf(world, body, primitive));
            sync.Bind(bindingOf(world, other, primitive));
        },
        "one writer"
    );
}
#endif
