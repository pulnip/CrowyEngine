#include <gtest/gtest.h>
#include "SceneParser.hpp"

using namespace Crowy;

TEST(SceneParser, ParseSimpleTransform){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.transform]
        position = [1, 2, 3]
        rotation = [0, 0, 0, 1]
        scale = [1, 1, 1]
    )";
    Vec3 position{1, 2, 3};
    auto rotation = unitQuat();
    auto scale = ones();

    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].transformIndex, -1);
    EXPECT_EQ(scene.transforms[0].position, position);
    EXPECT_EQ(scene.transforms[0].rotation, rotation);
    EXPECT_EQ(scene.transforms[0].scale, scale);
}
TEST(SceneParser, ParseSimpleMesh){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.mesh]
        id = "embedded:cube"
    )";

    std::string id = "embedded:cube";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].meshIndex, -1);
    EXPECT_EQ(scene.meshes[0].id, id);
}
TEST(SceneParser, ParseComplexMesh){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.mesh]
        id = "embedded:cube"
            [[entities.mesh.material_override]]
            baseColor = "embedded:red"
            targetSlot = "*"
            [entities.mesh.shader]
            module = "file:shader/Crowy.metallib"
            vsFunc = "vertex_main"
            fsFunc = "fragment_main"
    )";

    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].meshIndex, -1);
    EXPECT_EQ(scene.meshes[0].id, std::string("embedded:cube"));
    EXPECT_TRUE(scene.meshes[0].material_override.size() > 0);
    EXPECT_EQ(scene.meshes[0].material_override[0].baseColor, std::string("embedded:red"));
    EXPECT_EQ(scene.meshes[0].shader.module_, std::string("file:shader/Crowy.metallib"));
    EXPECT_EQ(scene.meshes[0].shader.vsFunc, std::string("vertex_main"));
    EXPECT_EQ(scene.meshes[0].shader.fsFunc, std::string("fragment_main"));
}

TEST(SceneParser, ParseEntityWithoutComponent){
    std::string tomlText = R"(
        [[entities]]
        name = "Light"
    )";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Light");
    EXPECT_EQ(scene.entities[0].transformIndex, -1);
}

TEST(SceneParser, ParseMultipleEntities){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.transform]
        position = [10, 20, 30]
        rotation = [0, 0, 0, 1]
        scale = [2, 2, 2]

        [[entities]]
        name = "Lamp"
    )";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 2);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_EQ(scene.entities[1].name, "Lamp");
    // First entity has a valid transform
    EXPECT_NE(scene.entities[0].transformIndex, -1);
    // Second entity does not have a transform
    EXPECT_EQ(scene.entities[1].transformIndex, -1);
    // Check transform values for the first entity
    ASSERT_FALSE(scene.transforms.empty());
    auto& tr = scene.transforms[scene.entities[0].transformIndex];
    EXPECT_EQ(tr.position, (Vec3{10, 20, 30}));
    EXPECT_EQ(tr.rotation, unitQuat());
    EXPECT_EQ(tr.scale, (Vec3{2, 2, 2}));
}

TEST(SceneParser, ParseMultipleProperties){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.transform]
        position = [10, 20, 30]
        rotation = [0, 0, 0, 1]
        scale = [2, 2, 2]
        [entities.mesh]
        id = "embedded:cube"
    )";
    std::string id = "embedded:cube";

    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].transformIndex, -1);
    ASSERT_FALSE(scene.transforms.empty());
    auto& tr = scene.transforms[scene.entities[0].transformIndex];
    EXPECT_EQ(tr.position, (Vec3{10, 20, 30}));
    EXPECT_EQ(tr.rotation, unitQuat());
    EXPECT_EQ(tr.scale, (Vec3{2, 2, 2}));
    ASSERT_FALSE(scene.meshes.empty());
    auto& msh = scene.meshes[scene.entities[0].meshIndex];
    EXPECT_EQ(msh.id, id);
}

TEST(SceneParser, ParseMultipleEntitiesWithMultipleProperties){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.transform]
        position = [10, 20, 30]
        rotation = [0, 0, 0, 1]
        scale = [2, 2, 2]
        [entities.mesh]
        id = "embedded:cube"

        [[entities]]
        name = "Lamp"
        [entities.transform]
        position = [15, 25, 35]
        rotation = [0, 0, 0, 1]
        scale = [1.5, 1.5, 1.5]
        [entities.mesh]
        id = "file:asset/lamp.fbx"
    )";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 2);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_EQ(scene.entities[1].name, "Lamp");
    EXPECT_NE(scene.entities[0].transformIndex, -1);
    EXPECT_NE(scene.entities[0].meshIndex, -1);
    EXPECT_NE(scene.entities[1].transformIndex, -1);
    EXPECT_NE(scene.entities[1].meshIndex, -1);

    ASSERT_FALSE(scene.transforms.empty());
    ASSERT_FALSE(scene.meshes.empty());

    auto& tr1 = scene.transforms[scene.entities[0].transformIndex];
    EXPECT_EQ(tr1.position, (Vec3{10, 20, 30}));
    EXPECT_EQ(tr1.rotation, unitQuat());
    EXPECT_EQ(tr1.scale, (Vec3{2, 2, 2}));
    auto& msh1 = scene.meshes[scene.entities[0].meshIndex];
    EXPECT_EQ(msh1.id, std::string("embedded:cube"));

    auto& tr2 = scene.transforms[scene.entities[1].transformIndex];
    EXPECT_EQ(tr2.position, (Vec3{15, 25, 35}));
    EXPECT_EQ(tr2.rotation, unitQuat());
    EXPECT_EQ(tr2.scale, (Vec3{1.5, 1.5, 1.5}));
    auto& msh2 = scene.meshes[scene.entities[1].meshIndex];
    EXPECT_EQ(msh2.id, std::string("file:asset/lamp.fbx"));
}

TEST(SceneParser, ThrowsOnInvalidVecLength){
    std::string tomlText = R"(
        [[entities]]
        name = "BadBox"
        [entities.transform]
        position = [1, 2]  # invalid length
    )";

    EXPECT_THROW({
        auto scene = parseSceneFromString(tomlText);
        (void)scene;
    }, std::runtime_error);
}
TEST(SceneParser, ThrowsOnInvalidMeshType){
    std::string tomlText = R"(
        [[entities]]
        name = "BadBox"
        [entities.mesh]
        id = 1.0
    )";

    EXPECT_THROW({
        auto scene = parseSceneFromString(tomlText);
        (void)scene;
    }, std::runtime_error);
}