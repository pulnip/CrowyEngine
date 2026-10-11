#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "LightComponent.hpp"
#include "LinearAlgebra.hpp"
#include "RenderScene.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    // a row Extract owns, overwritten by hand to see whether Extract writes
    // it again
    void poison(RenderScene& scene, u32 index) {
        scene.Lights().GetRef(scene.Lights().HandleAt(index)).intensity = -1.0f;
    }

    bool isPoisoned(const RenderScene& scene, u32 index) {
        return scene.Lights().At(index).intensity == -1.0f;
    }
}

// nothing here names a symbol of LightComponent.cpp, so only linking the
// module whole keeps its component and its extraction
TEST(LightComponent, RegistersWithoutAReference) {
    const auto* desc = ClassRegistry::FindType("LightComponent");
    ASSERT_TRUE(desc != nullptr);
    EXPECT_TRUE(ComponentRegistry::Get().Find(*desc) != nullptr);
}

TEST(LightComponent, RowsLandAtSyncInOrderWithEveryField) {
    RenderScene scene;
    World world;
    world.Provide(scene);

    world.Add(world.Spawn(), LightComponent{.castShadow = true});
    world.Add(
        world.Spawn(),
        LightComponent{
            .kind = LightKind::Spot,
            .enabled = false,
            .color = {1.0f, 0.5f, 0.25f},
            .intensity = 4.0f,
            .range = 3.0f,
            .innerConeAngle = 0.25f,
            .outerConeAngle = 0.5f,
            .shadowBias = 0.05f,
            .shadowNormalBias = 2.0f
        }
    );
    EXPECT_EQ(scene.Lights().Count(), 0u);

    world.Frame(0.0);
    ASSERT_EQ(scene.Lights().Count(), 2u);
    EXPECT_TRUE(scene.Lights().At(0).castShadow);

    const auto& spot = scene.Lights().At(1);
    EXPECT_EQ(spot.kind, LightKind::Spot);
    EXPECT_FALSE(spot.enabled);
    EXPECT_EQ(spot.color, (Vec3{1.0f, 0.5f, 0.25f}));
    EXPECT_EQ(spot.intensity, 4.0f);
    EXPECT_EQ(spot.range, 3.0f);
    EXPECT_EQ(spot.innerConeAngle, 0.25f);
    EXPECT_EQ(spot.outerConeAngle, 0.5f);
    EXPECT_EQ(spot.shadowBias, 0.05f);
    EXPECT_EQ(spot.shadowNormalBias, 2.0f);
}

// what a sample that aimed its rows by hand wrote, to the bit
TEST(LightComponent, ARootLightShinesAlongItsRotationFromItsPosition) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const Vec3 position{0.0f, 1.5f, 2.5f};
    const auto rotation = quat(rotateY(-0.5f), rotateX(1.0f));
    world.Add(
        world.Spawn(Transform{.position = position, .rotation = rotation}),
        LightComponent{}
    );

    world.Frame(0.0);
    const auto& row = scene.Lights().At(0);
    EXPECT_EQ(row.position, position);
    EXPECT_EQ(row.direction, forward(rotation));
}

TEST(LightComponent, AChildLightTurnsWithItsParent) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const auto turn = rotateY(0.75f);
    const auto parent = world.Spawn(Transform{.rotation = turn});
    world.Add(world.Spawn(Transform::Identity(), parent), LightComponent{});

    world.Frame(0.0);
    EXPECT_EQ(scene.Lights().At(0).direction, forward(turn));
}

TEST(LightComponent, OnlyAMovedOrWrittenLightRewritesItsRow) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const auto moved = world.Spawn();
    const auto written = world.Spawn();
    const auto quiet = world.Spawn();
    for(const auto actor: {moved, written, quiet})
        world.Add(actor, LightComponent{});
    world.Frame(0.0);

    for(u32 i = 0; i < 3; ++i)
        poison(scene, i);
    world.SetLocalTransform(moved, Transform{.position = {0.0f, 2.0f, 0.0f}});
    world.Write<LightComponent>(written)->intensity = 2.0f;
    world.Frame(0.0);

    EXPECT_FALSE(isPoisoned(scene, 0));
    EXPECT_EQ(scene.Lights().At(0).position, (Vec3{0.0f, 2.0f, 0.0f}));
    EXPECT_EQ(scene.Lights().At(1).intensity, 2.0f);
    EXPECT_TRUE(isPoisoned(scene, 2));
}

TEST(LightComponent, ADestroyedLightTakesOnlyItsRow) {
    RenderScene scene;
    World world;
    world.Provide(scene);
    const auto destroyed = world.Spawn();
    const auto kept = world.Spawn();
    world.Add(destroyed, LightComponent{.intensity = 1.0f});
    world.Add(kept, LightComponent{.intensity = 7.0f});
    world.Frame(0.0);

    world.Destroy(destroyed);
    world.Frame(0.0);
    ASSERT_EQ(scene.Lights().Count(), 1u);
    EXPECT_EQ(
        scene.Lights().GetRef(world.Get<LightComponent>(kept)->row).intensity,
        7.0f
    );
}
