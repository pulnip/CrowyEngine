#include <filesystem>
#include <utility>

#include <gtest/gtest.h>

#include "FileUtil.hpp"
#include "JsonLoader.hpp"
#include "LinearAlgebra.hpp"
#include "MaterialAsset.hpp"
#include "MeshAsset.hpp"
#include "MeshComponent.hpp"
#include "PathUtil.hpp"
#include "RenderCamera.hpp"
#include "RenderScene.hpp"
#include "Scene.hpp"
#include "StringUtil.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    AssetRef parsed(StrView text) {
        return AssetRef::Parse(text).value();
    }

    // every built-in is one slot of a unit box; any other path is a file
    // that is not there
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

    TextureStore::Loaded loadNoTexture(const AssetRef&) {
        return std::unexpected(Str("no device"));
    }

    // FrameHost's order: the scene's mesh rows hold mesh assets, a released
    // material leaves the scene, and the World lets go of everything first
    class MeshComponentTest: public testing::Test {
    protected:
        MeshStore meshes{&loadFakeMesh};
        TextureStore textures{&loadNoTexture};
        RenderScene scene;
        CameraSnapshot camera;
        MaterialStore materials{
            [this](const AssetRef& ref) {
                return loadMaterialAsset(
                    scene,
                    textures,
                    AssetRoots{.engine = toPath(CROWY_ENGINE_ROOT)},
                    ref
                );
            },
            [this](MaterialAsset asset) {
                releaseMaterialAsset(scene, std::move(asset));
            }
        };
        World world;

        void SetUp() override {
            world.Provide(scene);
            world.Provide(camera);
            world.Provide(meshes);
            world.Provide(materials);
        }

        Str savedBack(const std::filesystem::path& path) {
            loadSceneFile(world, path);
            world.Frame(0.0);

            return emitJson(saveScene(world), JsonStyle::Pretty) + "\n";
        }

        const AssetRef& drawnMesh(u32 primitive) const {
            const auto mesh = scene.Primitives().At(primitive).mesh;
            return scene.Meshes().GetRef(mesh).source.Ref();
        }
    };
}

// the components a scene file names, written back without one byte changed
TEST_F(MeshComponentTest, TheSampleSceneSavesBackAsItReads) {
    const auto path = toPath(CROWY_HELLO_SCENE_FILE);

    EXPECT_EQ(savedBack(path), readFileAsString(path));
}

TEST_F(MeshComponentTest, TheSampleSceneLightsItselfFromItsEnvironmentActor) {
    loadSceneFile(world, toPath(CROWY_HELLO_SCENE_FILE));
    world.Frame(0.0);

    const auto& environment = scene.Environment();
    EXPECT_EQ(environment.skyAmbient, (Vec3{0.123f, 0.174f, 0.262f}));
    EXPECT_EQ(environment.groundAmbient, (Vec3{0.033f, 0.024f, 0.016f}));
    ASSERT_TRUE(environment.sky.has_value());
    EXPECT_EQ(*environment.sky, (Vec3{0.139f, 0.212f, 0.356f}));
    ASSERT_EQ(scene.Lights().Count(), 1u);
    EXPECT_EQ(scene.Lights().At(0).kind, LightKind::Directional);
}

TEST_F(MeshComponentTest, ThePlaygroundSavesBackAsItReads) {
    const auto path = toPath(CROWY_PLAYGROUND_SCENE_FILE);

    EXPECT_EQ(savedBack(path), readFileAsString(path));
}

// a ref that names no file draws Missing, which only the picture shows
TEST_F(MeshComponentTest, EveryMaterialThePlaygroundNamesLoads) {
    loadSceneFile(world, toPath(CROWY_PLAYGROUND_SCENE_FILE));
    world.Frame(0.0);

    world.Each<MeshComponent>([&](ActorId actor, const MeshComponent& mesh) {
        SCOPED_TRACE(world.Name(actor));
        ASSERT_EQ(mesh.materialAssets.size(), mesh.materials.size());
        for(usize slot = 0; slot < mesh.materials.size(); ++slot) {
            EXPECT_EQ(
                mesh.materialAssets[slot].Ref().ToString(),
                mesh.materials[slot].ToString()
            );
        }
    });
}

TEST_F(MeshComponentTest, AnAssetThatFailsDrawsMissingAndKeepsItsRef) {
    const auto actor = world.Spawn();
    world.Add(
        actor,
        MeshComponent{
            .mesh = parsed("Props/Gone.fbx"),
            .materials = {parsed("Props/Gone.material")}
        }
    );
    world.Frame(0.0);

    ASSERT_EQ(scene.Primitives().Count(), 1u);
    EXPECT_EQ(drawnMesh(0).ToString(), "engine:Builtin/Missing");
    const auto& mesh = *world.Get<MeshComponent>(actor);
    ASSERT_EQ(mesh.materialAssets.size(), 1u);
    EXPECT_EQ(
        mesh.materialAssets[0].Ref().ToString(),
        "engine:Builtin/Missing"
    );

    const auto saved = saveScene(world);
    EXPECT_EQ(
        saved.get<Str>("actors[0].components[0].mesh"),
        Str("Props/Gone.fbx")
    );
    EXPECT_EQ(
        saved.get<Str>("actors[0].components[0].materials[0]"),
        Str("Props/Gone.material")
    );
}

TEST_F(MeshComponentTest, AWrittenRefRebuildsTheRowsAtSync) {
    const auto actor = world.Spawn();
    world.Add(actor, MeshComponent{.mesh = parsed("engine:Builtin/Box")});
    world.Frame(0.0);

    world.Write<MeshComponent>(actor)->mesh = parsed("engine:Builtin/Sphere");
    world.Frame(0.0);
    meshes.Collect();

    ASSERT_EQ(scene.Primitives().Count(), 1u);
    EXPECT_EQ(drawnMesh(0).ToString(), "engine:Builtin/Sphere");
    EXPECT_EQ(scene.Meshes().Count(), 1u);
    EXPECT_EQ(meshes.Count(), 1u);
}

TEST_F(MeshComponentTest, ACopyOfABuiltComponentBuildsRowsOfItsOwn) {
    const auto first = world.Spawn();
    world.Add(first, MeshComponent{.mesh = parsed("engine:Builtin/Box")});
    world.Frame(0.0);

    const auto second = world.Spawn();
    world.Add(second, *world.Get<MeshComponent>(first));
    world.Frame(0.0);
    ASSERT_EQ(scene.Primitives().Count(), 2u);
    EXPECT_NE(
        world.Get<MeshComponent>(first)->row,
        world.Get<MeshComponent>(second)->row
    );

    world.Destroy(first);
    world.Frame(0.0);
    EXPECT_EQ(scene.Primitives().Count(), 1u);
    EXPECT_EQ(scene.Meshes().Count(), 1u);
}

TEST_F(MeshComponentTest, ARemovedComponentLetsGoOfEveryAsset) {
    const auto actor = world.Spawn();
    world.Add(
        actor,
        MeshComponent{
            .mesh = parsed("engine:Builtin/Box"),
            .materials = {parsed("engine:Builtin/Translucent")}
        }
    );
    world.Frame(0.0);
    ASSERT_EQ(scene.Materials().Count(), 1u);

    world.Remove<MeshComponent>(actor);
    world.Frame(0.0);
    meshes.Collect();
    materials.Collect();

    EXPECT_EQ(scene.Primitives().Count(), 0u);
    EXPECT_EQ(scene.Meshes().Count(), 0u);
    EXPECT_EQ(scene.Materials().Count(), 0u);
    EXPECT_EQ(meshes.Count(), 0u);
    EXPECT_EQ(materials.Count(), 0u);
}
