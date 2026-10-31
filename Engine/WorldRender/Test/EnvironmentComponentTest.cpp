#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "EnvironmentComponent.hpp"
#include "LightComponent.hpp"
#include "LinearAlgebra.hpp"
#include "RenderScene.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    constexpr EnvironmentComponent Dusk{
        .skyAmbient = {0.1f, 0.2f, 0.3f},
        .groundAmbient = {0.4f, 0.5f, 0.6f},
        .skyColor = {0.7f, 0.8f, 0.9f}
    };
}

// nothing here names a symbol of EnvironmentComponent.cpp, so only linking
// the module whole keeps its component and its extraction
TEST(EnvironmentComponent, RegistersWithoutAReference) {
    const auto* desc = ClassRegistry::FindType("EnvironmentComponent");
    ASSERT_TRUE(desc != nullptr);
    EXPECT_TRUE(ComponentRegistry::Get().Find(*desc) != nullptr);
}

TEST(EnvironmentComponent, ReachesTheScenesEnvironment) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    world.Add(world.Spawn(), Dusk);

    world.Frame(0.0);
    EXPECT_EQ(scene.Environment().skyAmbient, Dusk.skyAmbient);
    EXPECT_EQ(scene.Environment().groundAmbient, Dusk.groundAmbient);
    ASSERT_TRUE(scene.Environment().sky.has_value());
    EXPECT_EQ(*scene.Environment().sky, Dusk.skyColor);
}

TEST(EnvironmentComponent, OnlyAWrittenEnvironmentRewritesTheScene) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const auto actor = world.Spawn();
    world.Add(actor, Dusk);
    world.Frame(0.0);

    scene.Environment().skyAmbient = ones();
    world.Frame(0.0);
    EXPECT_EQ(scene.Environment().skyAmbient, ones());

    world.Write<EnvironmentComponent>(actor)->skyAmbient = zeros();
    world.Frame(0.0);
    EXPECT_EQ(scene.Environment().skyAmbient, zeros());
}

TEST(EnvironmentComponent, AWorldWithoutOneLeavesTheEnvironmentAlone) {
    RenderScene scene;
    const EnvironmentSnapshot set{
        .skyAmbient = {0.5f, 0.5f, 0.5f},
        .groundAmbient = {0.25f, 0.25f, 0.25f}
    };
    scene.Environment() = set;
    World world;
    world.Provide(scene);
    world.Add(world.Spawn(), LightComponent{});

    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(scene.Environment().skyAmbient, set.skyAmbient);
    EXPECT_EQ(scene.Environment().groundAmbient, set.groundAmbient);
    EXPECT_FALSE(scene.Environment().sky.has_value());
}

TEST(EnvironmentComponent, AWorldWithoutOneNeedsNoScene) {
    World world;
    world.Spawn();

    world.Frame(0.0);
}

TEST(EnvironmentComponent, RemovingItClearsTheEnvironment) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const auto actor = world.Spawn();
    world.Add(actor, Dusk);
    world.Frame(0.0);

    world.Remove<EnvironmentComponent>(actor);
    world.Frame(0.0);
    EXPECT_EQ(scene.Environment().skyAmbient, zeros());
    EXPECT_FALSE(scene.Environment().sky.has_value());
}

TEST(EnvironmentComponent, TheSunIsTheLightOnTheSameActor) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const auto rotation = quat(rotateX(0.5f), rotateY(0.25f));
    const auto actor = world.Spawn(Transform{.rotation = rotation});
    world.Add(actor, Dusk);
    world.Add(actor, LightComponent{.intensity = 3.0f});

    world.Frame(0.0);
    ASSERT_EQ(scene.Lights().Count(), 1u);
    EXPECT_EQ(scene.Lights().At(0).direction, forward(rotation));
    EXPECT_EQ(scene.Lights().At(0).kind, LightKind::Directional);
    EXPECT_EQ(scene.Environment().skyAmbient, Dusk.skyAmbient);
}

TEST(EnvironmentComponentDeathTest, ASecondEnvironmentStops) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    world.Add(world.Spawn(), Dusk);
    world.Add(world.Spawn(), Dusk);

    EXPECT_DEATH(world.Frame(0.0), "a World has one environment");
}
