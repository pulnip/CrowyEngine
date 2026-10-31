#include <gtest/gtest.h>

#include "MaterialAsset.hpp"
#include "MeshAsset.hpp"
#include "MeshComponent.hpp"
#include "MeshRowComponent.hpp"
#include "PathUtil.hpp"
#include "PrimitiveActors.hpp"
#include "RenderScene.hpp"
#include "StringUtil.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    AssetRef parsed(StrView text) {
        return AssetRef::Parse(text).value();
    }

    // every built-in is one slot of a unit box
    MeshStore::Loaded loadFakeMesh(const AssetRef& ref) {
        StrView path = ref.Path();
        if(ref.Mount() != AssetMount::Engine ||
           !consumePrefix(path, "Builtin/"))
            return std::unexpected(Str("no such file"));

        return MeshAsset{
            .slots = {MeshAssetSlot{}},
            .bounds = {.center = zeros(), .halfScale = 0.5f * ones()}
        };
    }

    class PrimitiveActorsTest: public testing::Test {
    protected:
        MeshStore meshes{&loadFakeMesh};
        RenderScene scene;
        MaterialStore materials{
            [this](const AssetRef& ref) {
                return loadMaterialAsset(
                    scene.Materials(),
                    AssetRoots{.engine = toPath(CROWY_ENGINE_ROOT)},
                    ref
                );
            },
            [this](MaterialAsset asset) {
                releaseMaterialAsset(scene.Materials(), asset);
            }
        };
        PrimitiveActors actors;
        World world;

        void SetUp() override {
            provideStores(world);
            world.Provide(actors);
        }

        void provideStores(World& target) {
            target.Provide(scene);
            target.Provide(meshes);
            target.Provide(materials);
        }

        MeshHandle unitMesh() {
            return scene.Meshes().Add(
                MeshResource{
                    .localBounds = {.center = zeros(), .halfScale = ones()}
                }
            );
        }
    };
}

TEST_F(PrimitiveActorsTest, AMeshRowsPrimitiveNamesItsActor) {
    const auto actor = world.Spawn();
    world.Add(actor, MeshRowComponent{unitMesh()});
    world.Frame(0.0);

    const auto row = primitiveOfActor(world, actor);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(actors.ActorOf(*row), actor);
}

TEST_F(PrimitiveActorsTest, AMeshComponentsPrimitiveNamesItsActor) {
    world.Spawn();
    const auto actor = world.Spawn();
    world.Add(actor, MeshComponent{.mesh = parsed("engine:Builtin/Box")});
    world.Frame(0.0);

    const auto row = primitiveOfActor(world, actor);
    ASSERT_TRUE(row.has_value());
    EXPECT_EQ(actors.ActorOf(*row), actor);
}

// a gone actor's row is dropped, and the slot the next row reuses names only
// the newcomer
TEST_F(PrimitiveActorsTest, ARemovedActorsSlotReusedByAnotherIsNotMistaken) {
    const auto mesh = unitMesh();
    const auto first = world.Spawn();
    world.Add(first, MeshRowComponent{mesh});
    world.Frame(0.0);
    const auto gone = *primitiveOfActor(world, first);

    world.Destroy(first);
    world.Frame(0.0);
    EXPECT_FALSE(actors.ActorOf(gone).has_value());

    const auto second = world.Spawn();
    world.Add(second, MeshRowComponent{mesh});
    world.Frame(0.0);
    const auto reused = *primitiveOfActor(world, second);
    ASSERT_EQ(reused.GetIndex(), gone.GetIndex());
    EXPECT_FALSE(actors.ActorOf(gone).has_value());
    EXPECT_EQ(actors.ActorOf(reused), second);
}

TEST_F(PrimitiveActorsTest, ARebuiltComponentRebindsItsNewRow) {
    world.Spawn();
    const auto actor = world.Spawn();
    world.Add(actor, MeshComponent{.mesh = parsed("engine:Builtin/Box")});
    world.Frame(0.0);
    const auto before = *primitiveOfActor(world, actor);

    world.Write<MeshComponent>(actor)->mesh = parsed("engine:Builtin/Sphere");
    world.Frame(0.0);
    const auto after = *primitiveOfActor(world, actor);
    ASSERT_NE(after, before);
    EXPECT_EQ(actors.ActorOf(after), actor);
    EXPECT_FALSE(actors.ActorOf(before).has_value());
}

TEST_F(PrimitiveActorsTest, AWorldWithoutTheTableBuildsRowsAllTheSame) {
    World bare;
    provideStores(bare);
    const auto meshActor = bare.Spawn();
    bare.Add(meshActor, MeshComponent{.mesh = parsed("engine:Builtin/Box")});
    const auto rowActor = bare.Spawn();
    bare.Add(rowActor, MeshRowComponent{unitMesh()});
    bare.Frame(0.0);

    EXPECT_TRUE(primitiveOfActor(bare, meshActor).has_value());
    EXPECT_TRUE(primitiveOfActor(bare, rowActor).has_value());
    bare.Destroy(meshActor);
    bare.Destroy(rowActor);
    bare.Frame(0.0);
    EXPECT_EQ(scene.Primitives().Count(), 0u);
}
