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

TEST(CameraComponentDeathTest, ASecondCameraStops) {
    CameraSnapshot snapshot;
    World world;
    world.Provide(snapshot);
    world.Add(world.Spawn(), CameraComponent{});
    world.Add(world.Spawn(), CameraComponent{});

    EXPECT_DEATH(world.Frame(0.0), "a World is seen through one camera");
}
