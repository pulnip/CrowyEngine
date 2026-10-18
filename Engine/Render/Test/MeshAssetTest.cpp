#include <cstring>
#include <vector>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "GeometryPool.hpp"
#include "MeshAsset.hpp"
#include "MeshGenerator.hpp"
#include "ModelLoader.hpp"
#include "RenderScene.hpp"
#include "StringUtil.hpp"

using namespace Crowy;

namespace
{
    const auto testData = toPath(CROWY_MODEL_TEST_DATA);

    AssetRef parsed(StrView text) {
        return AssetRef::Parse(text).value();
    }

    bool sameVertices(const MeshData& a, const MeshData& b) {
        return a.vertices.size() == b.vertices.size() &&
               std::memcmp(
                   a.vertices.data(),
                   b.vertices.data(),
                   a.vertices.size() * sizeof(Vertex)
               ) == 0 &&
               a.indices == b.indices;
    }

    class MeshAssetTest: public testing::Test {
    protected:
        FakeDevice device;
        GeometryPool pool{device, 8192, 32768};
        AssetRoots roots{.engine = "unused", .content = testData};
    };
}

TEST_F(MeshAssetTest, TheBuiltInsComeFromCode) {
    const std::vector<std::pair<StrView, MeshData>> builtins{
        {"engine:Builtin/Box", MakeBox(0.5f)},
        {"engine:Builtin/Sphere", MakeSphere(0.5f, 32, 16)},
        {"engine:Builtin/Plane", MakePlane(Vec2{0.5f, 0.5f})},
    };

    for(const auto& [text, expected]: builtins) {
        const auto asset = loadMeshAsset(pool, roots, parsed(text));
        ASSERT_TRUE(asset.has_value()) << text << ": " << asset.error();
        ASSERT_EQ(asset->slots.size(), 1u) << text;

        const auto& slot = asset->slots[0];
        EXPECT_TRUE(sameVertices(slot.mesh, expected)) << text;
        EXPECT_EQ(slot.geometry.indexCount, expected.indices.size()) << text;
        EXPECT_TRUE(slot.material.empty()) << text;

        const auto bounds = boundsOf(expected.vertices);
        EXPECT_EQ(asset->bounds.center, bounds.center) << text;
        EXPECT_EQ(asset->bounds.halfScale, bounds.halfScale) << text;
    }
}

TEST_F(MeshAssetTest, AnUnknownBuiltInIsAnError) {
    const auto asset =
        loadMeshAsset(pool, roots, parsed("engine:Builtin/Cone"));

    ASSERT_FALSE(asset.has_value());
    EXPECT_EQ(asset.error(), "no built-in mesh 'Cone'");
}

// the Builtin folder is reserved under the engine mount only
TEST_F(MeshAssetTest, TheContentMountReadsBuiltinAsAFolder) {
    EXPECT_FALSE(loadMeshAsset(pool, roots, parsed("Builtin/Box")).has_value());
}

TEST_F(MeshAssetTest, AMissingFileIsAnError) {
    const auto asset = loadMeshAsset(pool, roots, parsed("Missing.fbx"));

    ASSERT_FALSE(asset.has_value());
    EXPECT_FALSE(asset.error().empty());
}

TEST_F(MeshAssetTest, AModelFileKeepsEverySlot) {
    const auto model = loadModel(testData / "AxisProbe.fbx");
    const auto asset = loadMeshAsset(pool, roots, parsed("AxisProbe.fbx"));
    ASSERT_TRUE(asset.has_value()) << asset.error();

    ASSERT_EQ(asset->slots.size(), model.slots.size());
    for(usize i = 0; i < model.slots.size(); ++i) {
        EXPECT_EQ(asset->slots[i].material, model.slots[i].material);
        EXPECT_TRUE(sameVertices(asset->slots[i].mesh, model.slots[i].mesh));
    }
    EXPECT_EQ(asset->bounds.center, model.bounds.center);
    EXPECT_EQ(asset->bounds.halfScale, model.bounds.halfScale);
}

TEST(MeshAsset, ASlotThatDoesNotFitGivesBackTheOnesThatDid) {
    ModelData model;
    model.slots.push_back(
        ModelSlot{.material = "small", .mesh = MakeBox(0.5f)}
    );
    model.slots.push_back(
        ModelSlot{.material = "large", .mesh = MakeSphere(0.5f, 32, 16)}
    );
    // the box's 36 indices fit, the sphere's 2880 do not
    FakeDevice device;
    GeometryPool pool{device, 8192, 1024};

    const auto asset = uploadMeshAsset(pool, std::move(model));
    ASSERT_FALSE(asset.has_value());
    EXPECT_TRUE(asset.error().starts_with("geometry pool out of index space"))
        << asset.error();
    EXPECT_EQ(device.deferred.size(), 1u);
}

TEST(MeshAsset, AModelWithNoSlotsIsAnError) {
    FakeDevice device;
    GeometryPool pool{device, 64, 256};

    const auto asset = uploadMeshAsset(pool, ModelData{});
    ASSERT_FALSE(asset.has_value());
    EXPECT_EQ(asset.error(), "the model holds no triangles");
}

TEST_F(MeshAssetTest, AReleaseFreesEverySlotOnceTheGPUIsDone) {
    auto asset = loadMeshAsset(pool, roots, parsed("AxisProbe.fbx"));
    ASSERT_TRUE(asset.has_value()) << asset.error();
    const auto slots = asset->slots.size();

    releaseMeshAsset(pool, std::move(*asset));
    EXPECT_EQ(device.deferred.size(), slots);

    device.RunDeferred();
    EXPECT_TRUE(device.deferred.empty());
}

namespace
{
    // the store goes after the scene, whose rows hold its handles
    class MeshRowTest: public testing::Test {
    protected:
        FakeDevice device;
        GeometryPool pool{device, 8192, 32768};
        MeshStore store{
            [this](const AssetRef& ref) {
                return loadMeshAsset(pool, AssetRoots{}, ref);
            },
            [this](MeshAsset asset) {
                releaseMeshAsset(pool, std::move(asset));
            }
        };
        RenderScene scene;
        MaterialHandle material = scene.Materials().Add(MaterialResource{});

        MeshHandle AddBox() {
            return scene.Meshes().Add(meshResourceOf(
                store.Load(parsed("engine:Builtin/Box")).value(),
                {material}
            ));
        }
    };
}

TEST_F(MeshRowTest, ARowDrawsEverySlotOfItsAsset) {
    const auto row = scene.Meshes().GetRef(AddBox());

    ASSERT_EQ(row.subMeshes.size(), 1u);
    EXPECT_EQ(row.subMeshes[0].geometry.indexCount, 36u);
    EXPECT_EQ(row.subMeshes[0].materialSlot, 0u);
    EXPECT_EQ(row.localBounds.halfScale, (Vec3{0.5f, 0.5f, 0.5f}));
    EXPECT_EQ(row.source.Ref(), parsed("engine:Builtin/Box"));
}

TEST_F(MeshRowTest, TheAssetStaysWhileARowDrawsIt) {
    const auto row = AddBox();

    store.Collect();
    EXPECT_EQ(store.Count(), 1u);
    EXPECT_TRUE(device.deferred.empty());

    scene.Meshes().Remove(row);
    store.Collect();
    EXPECT_EQ(store.Count(), 0u);
    EXPECT_EQ(device.deferred.size(), 1u);
}

TEST_F(MeshRowTest, RowsThatShareAnAssetKeepItUntilTheLastGoes) {
    const auto first = AddBox();
    const auto second = AddBox();

    scene.Meshes().Remove(first);
    store.Collect();
    EXPECT_EQ(store.Count(), 1u);

    scene.Meshes().Remove(second);
    store.Collect();
    EXPECT_EQ(store.Count(), 0u);
}

TEST_F(MeshRowTest, AClearedSceneLetsItsAssetsGo) {
    AddBox();

    scene.Clear();
    store.Collect();
    EXPECT_EQ(store.Count(), 0u);
}
