#include <vector>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "LinearAlgebra.hpp"
#include "MeshComponent.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    constexpr AABB3D UnitBox{.center = zeros(), .halfScale = 0.5f * ones()};

    MeshHandle addUnitMesh(RenderScene& scene) {
        return scene.Meshes().Add(MeshResource{.localBounds = UnitBox});
    }

    Transform at(Vec3 position) {
        return Transform{.position = position};
    }
}

// nothing here names a symbol of MeshComponent.cpp, so only linking the
// module whole keeps its component and its extraction
TEST(MeshComponent, RegistersWithoutAReference) {
    const auto* desc = ClassRegistry::FindType("MeshComponent");
    ASSERT_TRUE(desc != nullptr);
    EXPECT_TRUE(ComponentRegistry::Get().Find(*desc) != nullptr);
}

TEST(MeshComponent, RowsLandAtSyncInTheOrderTheMeshesWereAdded) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);

    for(i32 i = 0; i < 3; ++i)
        world.Add(world.Spawn(at({f32(i), 0.0f, 0.0f})), MeshComponent{mesh});
    EXPECT_EQ(scene.Primitives().Count(), 0u);

    world.Frame(0.0);
    ASSERT_EQ(scene.Primitives().Count(), 3u);
    for(u32 i = 0; i < 3; ++i) {
        EXPECT_EQ(
            scene.Primitives().At(i).worldBounds.center,
            (Vec3{f32(i), 0.0f, 0.0f})
        );
    }
}

TEST(MeshComponent, TheExtractAfterAMoveWritesTheRowFromTheActor) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    const auto actor = world.Spawn();
    world.Add(actor, MeshComponent{mesh});
    world.Frame(0.0);

    world.SetLocalTransform(actor, at({0.0f, 4.0f, 0.0f}));
    world.Frame(0.0);
    EXPECT_EQ(
        scene.Primitives().At(0).localToWorld[3],
        (Vec4{0.0f, 4.0f, 0.0f, 1.0f})
    );
}

TEST(MeshComponent, FlagsReachTheRow) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    world.Add(
        world.Spawn(),
        MeshComponent{.mesh = mesh, .flags = PrimitiveFlags::Visible}
    );

    world.Frame(0.0);
    EXPECT_EQ(scene.Primitives().At(0).flags, PrimitiveFlags::Visible);
}

TEST(MeshComponent, ADestroyedActorTakesItsRow) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    std::vector<ActorId> actors;
    for(i32 i = 0; i < 3; ++i) {
        actors.push_back(world.Spawn());
        world.Add(actors.back(), MeshComponent{mesh});
    }
    world.Frame(0.0);

    world.Destroy(actors[1]);
    world.Frame(0.0);
    EXPECT_EQ(scene.Primitives().Count(), 2u);
}

// what a sample that placed its rows by hand wrote, to the bit
TEST(MeshComponent, ATranslatedActorWritesTheRowATranslationWould) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    const Vec3 position{45.0f, 95.0f, 120.0f};
    world.Add(world.Spawn(at(position)), MeshComponent{mesh});

    world.Frame(0.0);
    const auto& row = scene.Primitives().At(0);
    EXPECT_EQ(row.localToWorld, translateMat(position));
    EXPECT_EQ(row.worldBounds.center, position);
    EXPECT_EQ(row.worldBounds.halfScale, 0.5f * ones());
}

namespace
{
    // a row Extract owns, overwritten by hand to see whether Extract writes
    // it again
    void poison(RenderScene& scene, u32 index) {
        scene.Primitives().GetRef(scene.Primitives().HandleAt(index)).flags =
            PrimitiveFlags::None;
    }

    bool isPoisoned(const RenderScene& scene, u32 index) {
        return scene.Primitives().At(index).flags == PrimitiveFlags::None;
    }
}

TEST(MeshComponent, AFrameWhereNothingChangedWritesNoRow) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    world.Add(world.Spawn(), MeshComponent{mesh});
    world.Frame(0.0);

    poison(scene, 0);
    world.Frame(0.0);
    EXPECT_TRUE(isPoisoned(scene, 0));
}

TEST(MeshComponent, AMovedActorRewritesItsRowAndThoseBelowItOnly) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    const auto parent = world.Spawn();
    world.Add(parent, MeshComponent{mesh});
    world.Add(world.Spawn(Transform::Identity(), parent), MeshComponent{mesh});
    world.Add(world.Spawn(), MeshComponent{mesh});
    world.Frame(0.0);

    for(u32 i = 0; i < 3; ++i)
        poison(scene, i);
    world.SetLocalTransform(parent, at({0.0f, 2.0f, 0.0f}));
    world.Frame(0.0);

    EXPECT_FALSE(isPoisoned(scene, 0));
    EXPECT_FALSE(isPoisoned(scene, 1));
    EXPECT_TRUE(isPoisoned(scene, 2));
    EXPECT_EQ(
        scene.Primitives().At(1).worldBounds.center,
        (Vec3{0.0f, 2.0f, 0.0f})
    );
}

TEST(MeshComponent, AWrittenComponentRewritesItsRow) {
    RenderScene scene;
    const auto mesh = addUnitMesh(scene);
    World world;
    world.Provide(scene);
    const auto actor = world.Spawn();
    world.Add(actor, MeshComponent{mesh});
    world.Frame(0.0);

    world.Write<MeshComponent>(actor)->flags = PrimitiveFlags::CastShadow;
    world.Frame(0.0);
    EXPECT_EQ(scene.Primitives().At(0).flags, PrimitiveFlags::CastShadow);
}
