#include <gtest/gtest.h>

#include "RenderScene.hpp"

using namespace Crowy;

namespace
{
    MaterialHandle AddMaterial(RenderScene& scene, Vec3 albedo) {
        return scene.Materials().Add(
            MaterialResource{.data = {.albedo = albedo}}
        );
    }

    MeshHandle AddMesh(
        RenderScene& scene,
        MaterialHandle material,
        u32 subMeshes
    ) {
        MeshResource mesh{.materials = {material}};
        for(u32 i = 0; i < subMeshes; ++i) {
            mesh.subMeshes.push_back(SubMesh{.geometry = {.indexCount = 3}});
        }

        return scene.Meshes().Add(mesh);
    }
}

// A material's row index is what DrawData::materialIndex carries, so the three
// tables have to stay consistent through a removal.
TEST(RenderScene, MaterialRowFollowsARemoval) {
    RenderScene scene;

    const auto first = AddMaterial(scene, {1.0f, 0.0f, 0.0f});
    const auto middle = AddMaterial(scene, {0.0f, 1.0f, 0.0f});
    const auto last = AddMaterial(scene, {0.0f, 0.0f, 1.0f});

    EXPECT_EQ(scene.Materials().IndexOf(last), 2u);

    scene.Materials().Remove(middle);

    EXPECT_EQ(scene.Materials().IndexOf(first), 0u);
    EXPECT_EQ(scene.Materials().IndexOf(last), 1u);
    EXPECT_EQ(scene.Materials().At(1).data.albedo.z, 1.0f);
}

TEST(RenderScene, MeshKeepsItsSubMeshesAndMaterial) {
    RenderScene scene;

    const auto material = AddMaterial(scene, {1.0f, 1.0f, 1.0f});
    const auto mesh = AddMesh(scene, material, 5);

    const auto& stored = scene.Meshes().Read(mesh);
    ASSERT_EQ(stored.subMeshes.size(), 5u);
    ASSERT_EQ(stored.materials.size(), 1u);
    EXPECT_EQ(scene.Materials().IndexOf(stored.materials[0]), 0u);
}

TEST(RenderScene, PrimitiveReferencesAMesh) {
    RenderScene scene;

    const auto material = AddMaterial(scene, {1.0f, 1.0f, 1.0f});
    const auto mesh = AddMesh(scene, material, 1);
    const auto primitive =
        scene.Primitives().Add(PrimitiveSnapshot{.mesh = mesh});

    EXPECT_TRUE(
        scene.Meshes().IsValid(scene.Primitives().Read(primitive).mesh)
    );
}

TEST(RenderScene, ClearEmptiesEveryTable) {
    RenderScene scene;

    const auto material = AddMaterial(scene, {1.0f, 1.0f, 1.0f});
    const auto mesh = AddMesh(scene, material, 1);
    scene.Primitives().Add(PrimitiveSnapshot{.mesh = mesh});

    scene.Clear();

    EXPECT_TRUE(scene.Primitives().IsEmpty());
    EXPECT_TRUE(scene.Meshes().IsEmpty());
    EXPECT_TRUE(scene.Materials().IsEmpty());
}
