#include <cmath>

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

    struct Pendulum {
        BodyHandle bob;
        HingeHandle hinge;
    };

    // a bob hung off center from a pivot, turning about z
    Pendulum hangPendulum(PhysicsWorld& world, Vec3 pivot) {
        const auto bob = world.CreateBody(BodyDesc{
            .shape = SphereShape{0.1f},
            .pose = BodyPose{.position = pivot + Vec3{0.6f, -0.8f, 0.0f}},
            .motion = BodyMotion::Dynamic,
            .mass = 1.0f,
        });
        const auto hinge = world.CreateHinge(HingeDesc{
            .body = bob,
            .pivot = pivot,
            .axis = unitZ(),
            .normal = unitX(),
        });

        return Pendulum{.bob = bob, .hinge = hinge};
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

TEST(BodyPrimitiveSync, CompoundPartsFollowTheirBody) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    const auto part = CompoundPart{
        .halfExtent = {0.4f, 0.05f, 0.2f},
        .pose = BodyPose{.position = {0.5f, 0.25f, 0.0f}},
    };
    const auto body = world.CreateBody(BodyDesc{
        .shape = CompoundShape{{part}},
        .pose = BodyPose{.position = {1.0f, 2.0f, 3.0f}, .rotation = Tilted},
        .motion = BodyMotion::Static,
    });
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(Placement);
    sync.Bind(BodyBinding{
        .body = body,
        .primitive = primitive,
        .part = part.pose,
        .meshScale = 2.0f * part.halfExtent,
    });

    sync.Sync(world, scene);
    const auto pose = world.PoseOf(body);
    const auto expected = translateMat(Placement) *
        modelMat(pose.position, pose.rotation, ones()) *
        modelMat(
            part.pose.position,
            part.pose.rotation,
            2.0f * part.halfExtent
        );
    const auto& written = scene.Primitives().GetRef(primitive);
    for(usize column = 0; column < 4; ++column)
        EXPECT_EQ(written.localToWorld[column], expected[column]);
    // the part's center, carried by the body's turn
    EXPECT_EQ(
        written.worldBounds.center,
        static_cast<Vec3>(expected * Vec4{0.0f, 0.0f, 0.0f, 1.0f})
    );
    EXPECT_EQ(world.BodyCount(), 1u);
    EXPECT_EQ(world.TickCount(), 0u);
}

TEST(BodyPrimitiveSync, TetherSpansAnchorToBody) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    constexpr Vec3 Pivot{0.25f, 2.0f, 0.5f};
    const auto [bob, hinge] = hangPendulum(world, Pivot);
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(Placement);
    sync.Bind(TetherBinding{
        .hinge = hinge,
        .body = bob,
        .primitive = primitive,
        .anchor = Pivot,
        .axis = unitZ(),
        .thickness = 0.03f,
    });

    for(int i = 0; i < 20; ++i)
        world.Step();
    sync.Sync(world, scene);
    const auto end = world.PoseOf(bob).position;
    const auto& written = scene.Primitives().GetRef(primitive);
    // the unit box's height runs from the pivot to the bob
    EXPECT_EQ(static_cast<Vec3>(written.localToWorld[1]), end - Pivot);
    EXPECT_EQ(
        static_cast<Vec3>(written.localToWorld[3]),
        Placement + (Pivot + end) * 0.5f
    );
    // three centimeters across and along the axis, whatever the rod's length
    const auto across = static_cast<Vec3>(written.localToWorld[0]);
    EXPECT_NEAR(norm(across), 0.03f, 1.0e-6f);
    EXPECT_NEAR(dot(across, end - Pivot), 0.0f, 1.0e-6f);
    EXPECT_EQ(across.z, 0.0f);
    EXPECT_EQ(
        static_cast<Vec3>(written.localToWorld[2]),
        (Vec3{0.0f, 0.0f, 0.03f})
    );
    const auto bounds = transformAABB3D(written.localToWorld, UnitMeshBounds);
    EXPECT_EQ(written.worldBounds.center, bounds.center);
    EXPECT_EQ(written.worldBounds.halfScale, bounds.halfScale);
    EXPECT_EQ(
        written.flags,
        combine(PrimitiveFlags::Visible, PrimitiveFlags::CastShadow)
    );
    // still on its circle, so the rod has the hinge's length
    EXPECT_NEAR(norm(end - Pivot), 1.0f, 1.0e-3f);
    EXPECT_EQ(world.TickCount(), 20u);
    EXPECT_EQ(world.BodyCount(), 1u);
}

TEST(BodyPrimitiveSync, TetherHidesOnRelease) {
    PhysicsRuntime runtime;
    PhysicsWorld world(runtime);
    RenderScene scene;
    const auto [bob, hinge] = hangPendulum(world, Vec3{0.0f, 2.0f, 0.0f});
    const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
    BodyPrimitiveSync sync(Placement);
    sync.Bind(TetherBinding{
        .hinge = hinge,
        .body = bob,
        .primitive = primitive,
        .anchor = {0.0f, 2.0f, 0.0f},
    });
    world.Step();
    sync.Sync(world, scene);
    const auto held = scene.Primitives().GetRef(primitive).localToWorld;

    world.ReleaseHinge(hinge);
    for(int i = 0; i < 10; ++i)
        world.Step();
    sync.Sync(world, scene);
    const auto& written = scene.Primitives().GetRef(primitive);
    EXPECT_EQ(written.flags, PrimitiveFlags::None);
    for(usize column = 0; column < 4; ++column)
        EXPECT_EQ(written.localToWorld[column], held[column]);
    EXPECT_EQ(world.TickCount(), 11u);
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

TEST(BodyPrimitiveSyncDeathTest, OneWriterAcrossTethers) {
    EXPECT_DEATH(
        {
            PhysicsRuntime runtime;
            PhysicsWorld world(runtime);
            RenderScene scene;
            const auto pendulum = hangPendulum(world, zeros());
            const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{});
            BodyPrimitiveSync sync(zeros());
            sync.Bind(bindingOf(world, pendulum.bob, primitive));
            sync.Bind(TetherBinding{
                .hinge = pendulum.hinge,
                .body = pendulum.bob,
                .primitive = primitive,
            });
        },
        "one writer"
    );
}
#endif
