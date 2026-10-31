#include <gtest/gtest.h>

#include "CameraComponent.hpp"
#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "LinearAlgebra.hpp"
#include "RenderCamera.hpp"
#include "World.hpp"

using namespace Crowy;

// nothing here names a symbol of CameraComponent.cpp, so only linking the
// module whole keeps its component and its extraction
TEST(CameraComponent, RegistersWithoutAReference) {
    const auto* desc = ClassRegistry::FindType("CameraComponent");
    ASSERT_TRUE(desc != nullptr);
    EXPECT_TRUE(ComponentRegistry::Get().Find(*desc) != nullptr);
}

// what a camera that kept its own pose computed, to the bit
TEST(CameraComponent, ARootCameraViewsFromItsPose) {
    CameraSnapshot snapshot;
    World world;
    world.Provide(snapshot);
    const Vec3 position{0.0f, 1.7f, -5.0f};
    const auto rotation = quat(rotateY(0.25f), rotateX(0.1f));
    world.Add(
        world.Spawn(Transform{.position = position, .rotation = rotation}),
        CameraComponent{.fovY = 1.0f, .nearZ = 0.05f, .farZ = 100.0f}
    );

    world.Frame(0.0);
    EXPECT_EQ(snapshot.view, viewMat(position, rotation));
    EXPECT_EQ(snapshot.position, position);
    EXPECT_EQ(snapshot.fovY, 1.0f);
    EXPECT_EQ(snapshot.nearZ, 0.05f);
    EXPECT_EQ(snapshot.farZ, 100.0f);
}

TEST(CameraComponent, OnlyAMovedOrWrittenCameraRewritesTheSnapshot) {
    CameraSnapshot snapshot;
    World world;
    world.Provide(snapshot);
    const auto camera = world.Spawn();
    world.Add(camera, CameraComponent{});
    world.Frame(0.0);

    snapshot.fovY = -1.0f;
    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, -1.0f);

    world.SetLocalTransform(camera, Transform{.position = {0.0f, 0.0f, 1.0f}});
    world.Frame(0.0);
    EXPECT_EQ(snapshot.position, (Vec3{0.0f, 0.0f, 1.0f}));

    world.Write<CameraComponent>(camera)->fovY = 0.5f;
    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, 0.5f);
}

TEST(CameraComponent, AWorldWithoutACameraNeedsNoSnapshot) {
    World world;
    world.Spawn();

    world.Frame(0.0);
}

namespace
{
    // a camera the scene file knows as `fileId`
    ActorId addCamera(World& world, i64 fileId, f32 fovY) {
        const auto actor = world.Spawn();
        world.SetFileId(actor, fileId);
        world.Add(actor, CameraComponent{.fovY = fovY});

        return actor;
    }
}

TEST(CameraComponent, TheLowestFileIdIsTheOneSeen) {
    CameraSnapshot snapshot;
    World world;
    world.Provide(snapshot);
    addCamera(world, 7, 0.7f);
    addCamera(world, 3, 0.3f);
    addCamera(world, 5, 0.5f);

    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, 0.3f);
}

TEST(CameraComponent, AnotherCameraChangingWritesNothing) {
    CameraSnapshot snapshot;
    World world;
    world.Provide(snapshot);
    const auto active = addCamera(world, 1, 0.1f);
    const auto other = addCamera(world, 2, 0.2f);
    world.Frame(0.0);

    snapshot.fovY = -1.0f;
    world.SetLocalTransform(other, Transform{.position = {0.0f, 0.0f, 1.0f}});
    world.Write<CameraComponent>(other)->fovY = 0.9f;
    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, -1.0f);
    EXPECT_EQ(snapshot.position, zeros());

    world.Write<CameraComponent>(active)->fovY = 0.4f;
    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, 0.4f);
}

TEST(CameraComponent, ACameraWithALowerFileIdTakesOverWhenItLands) {
    CameraSnapshot snapshot;
    World world;
    world.Provide(snapshot);
    addCamera(world, 5, 0.5f);
    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, 0.5f);

    addCamera(world, 2, 0.2f);
    world.Frame(0.0);
    EXPECT_EQ(snapshot.fovY, 0.2f);
}
