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
    EXPECT_NE(scene.entities[0].transformIndex, INVALID_INDEX);
    EXPECT_EQ(scene.transformSpecs[0].position, position);
    EXPECT_EQ(scene.transformSpecs[0].rotation, rotation);
    EXPECT_EQ(scene.transformSpecs[0].scale, scale);
}
TEST(SceneParser, ParseSimpleMesh){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.renderObject]
        uri = "embedded:cube"
    )";

    std::string uri = "embedded:cube";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].renderObjectIndex, INVALID_INDEX);
    EXPECT_EQ(scene.renderObjectSpecs[0].uri, uri);
}
TEST(SceneParser, ParseComplexMesh){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.renderObject]
        uri = "embedded:cube"
        renderType = "unlit"
            [[entities.renderObject.material_override]]
            baseColor = "embedded:red"
            targetSlot = "*"
    )";

    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].renderObjectIndex, INVALID_INDEX);
    EXPECT_EQ(scene.renderObjectSpecs[0].uri, std::string("embedded:cube"));
    EXPECT_EQ(scene.renderObjectSpecs[0].renderType, std::string("unlit"));
    EXPECT_TRUE(scene.renderObjectSpecs[0].material_override.size() > 0);
    EXPECT_EQ(scene.renderObjectSpecs[0].material_override[0].baseColor, std::string("embedded:red"));
}

TEST(SceneParser, ParseEntityWithoutComponent){
    std::string tomlText = R"(
        [[entities]]
        name = "Light"
    )";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Light");
    EXPECT_EQ(scene.entities[0].transformIndex, INVALID_INDEX);
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
    EXPECT_NE(scene.entities[0].transformIndex, INVALID_INDEX);
    // Second entity does not have a transform
    EXPECT_EQ(scene.entities[1].transformIndex, INVALID_INDEX);
    // Check transform values for the first entity
    ASSERT_FALSE(scene.transformSpecs.empty());
    auto& tr = scene.transformSpecs[scene.entities[0].transformIndex];
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
        [entities.renderObject]
        uri = "embedded:cube"
    )";
    std::string uri = "embedded:cube";

    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 1);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_NE(scene.entities[0].transformIndex, INVALID_INDEX);
    ASSERT_FALSE(scene.transformSpecs.empty());
    auto& tr = scene.transformSpecs[scene.entities[0].transformIndex];
    EXPECT_EQ(tr.position, (Vec3{10, 20, 30}));
    EXPECT_EQ(tr.rotation, unitQuat());
    EXPECT_EQ(tr.scale, (Vec3{2, 2, 2}));
    ASSERT_FALSE(scene.renderObjectSpecs.empty());
    auto& msh = scene.renderObjectSpecs[scene.entities[0].renderObjectIndex];
    EXPECT_EQ(msh.uri, uri);
}

TEST(SceneParser, ParseMultipleEntitiesWithMultipleProperties){
    std::string tomlText = R"(
        [[entities]]
        name = "Box"
        [entities.transform]
        position = [10, 20, 30]
        rotation = [0, 0, 0, 1]
        scale = [2, 2, 2]
        [entities.renderObject]
        uri = "embedded:cube"

        [[entities]]
        name = "Lamp"
        [entities.transform]
        position = [15, 25, 35]
        rotation = [0, 0, 0, 1]
        scale = [1.5, 1.5, 1.5]
        [entities.renderObject]
        uri = "file:asset/lamp.fbx"
    )";
    auto scene = parseSceneFromString(tomlText);

    ASSERT_EQ(scene.entities.size(), 2);
    EXPECT_EQ(scene.entities[0].name, "Box");
    EXPECT_EQ(scene.entities[1].name, "Lamp");
    EXPECT_NE(scene.entities[0].transformIndex, INVALID_INDEX);
    EXPECT_NE(scene.entities[0].renderObjectIndex, INVALID_INDEX);
    EXPECT_NE(scene.entities[1].transformIndex, INVALID_INDEX);
    EXPECT_NE(scene.entities[1].renderObjectIndex, INVALID_INDEX);

    ASSERT_FALSE(scene.transformSpecs.empty());
    ASSERT_FALSE(scene.renderObjectSpecs.empty());

    auto& tr1 = scene.transformSpecs[scene.entities[0].transformIndex];
    EXPECT_EQ(tr1.position, (Vec3{10, 20, 30}));
    EXPECT_EQ(tr1.rotation, unitQuat());
    EXPECT_EQ(tr1.scale, (Vec3{2, 2, 2}));
    auto& msh1 = scene.renderObjectSpecs[scene.entities[0].renderObjectIndex];
    EXPECT_EQ(msh1.uri, std::string("embedded:cube"));

    auto& tr2 = scene.transformSpecs[scene.entities[1].transformIndex];
    EXPECT_EQ(tr2.position, (Vec3{15, 25, 35}));
    EXPECT_EQ(tr2.rotation, unitQuat());
    EXPECT_EQ(tr2.scale, (Vec3{1.5, 1.5, 1.5}));
    auto& msh2 = scene.renderObjectSpecs[scene.entities[1].renderObjectIndex];
    EXPECT_EQ(msh2.uri, std::string("file:asset/lamp.fbx"));
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
        [entities.renderObject]
        uri = 1.0
    )";

    EXPECT_THROW({
        auto scene = parseSceneFromString(tomlText);
        (void)scene;
    }, std::runtime_error);
}