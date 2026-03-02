#include "EntityRegistry.hpp"
#include "SceneModuleTest.hpp"
#include "SceneLoader.hpp"
#include "ConfigParser.hpp"

using Crowy::parseSceneFromString;
using Crowy::loadScene, Crowy::EntityRegistry;
using Crowy::TransformComponent, Crowy::RenderObjectComponent;

TEST_F(SceneModuleTest, EntityWithTransformComponent){
    std::string tomlText = R"(
        [[entities]]
        name = "Entity1"
        [entities.transform]
        position = [0, 0, 0]
        rotation = [0, 0, 0, 1]
        scale = [1, 1, 1]

        [[entities]]
        name = "Entity2"
        [entities.transform]
        position = [1, 2, 3]
        rotation = [0, 0, 0, 1]
        scale = [1, 1, 1]

        [[entities]]
        name = "Entity3"
        [entities.transform]
        position = [5, 5, 5]
        rotation = [0, 0, 0, 1]
        scale = [2, 2, 2]
    )";
    TransformComponent expectedComponents[] = {
        {
            .position = Crowy::zeros(),
            .rotation = Crowy::unit_quat(),
            .scale = Crowy::ones()
        },
        {
            .position = Crowy::Vec3{1, 2, 3},
            .rotation = Crowy::unit_quat(),
            .scale = Crowy::ones()
        },
        {
            .position = Crowy::Vec3{5, 5, 5},
            .rotation = Crowy::unit_quat(),
            .scale = Crowy::Vec3{2, 2, 2}
        }
    };

    auto sceneSpec = parseSceneFromString(tomlText);

    EntityRegistry registry;
    loadScene(sceneSpec, registry);

    int entityCount = 0;
    for(const auto& [id, bit, transform]: registry.query<TransformComponent>()){
        EXPECT_EQ(bit, Crowy::bits_of<TransformComponent>());

        const auto& expected = expectedComponents[entityCount];
        EXPECT_EQ(transform.position, expected.position);
        EXPECT_EQ(transform.rotation, expected.rotation);
        EXPECT_EQ(transform.scale   ,    expected.scale);

        ++entityCount;
    }

    EXPECT_EQ(entityCount, 3);
}

TEST_F(SceneModuleTest, EntityWithRenderObjectComponent){
    std::string tomlText = R"(
        [[entities]]
        name = "Cube"
        [entities.transform]
        position = [0, 0, 0]
        rotation = [0, 0, 0, 1]
        scale = [1, 1, 1]
        [entities.renderObject]
        uri = "embedded:cube"
        renderType = "unlit"
        [[entities.renderObject.material_override]]
            baseColor = "embedded:red"
            targetSlot = "*"
    )";

    auto sceneSpec = parseSceneFromString(tomlText);

    EntityRegistry registry;
    loadScene(sceneSpec, registry);

    int entityCount = 0;
    for(const auto& [id, bit, transform, renderObject]
        : registry.query<TransformComponent, RenderObjectComponent>()
    ){
        EXPECT_EQ(transform.position, Crowy::zeros());
        EXPECT_EQ(transform.rotation, Crowy::unit_quat());
        EXPECT_EQ(transform.scale   , Crowy::ones());

        EXPECT_EQ(renderObject.renderType, std::hash<Crowy::RenderType>()("unlit"));
        EXPECT_TRUE(renderObject.mesh.isValid());
        EXPECT_TRUE(renderObject.materialSet.isValid());

        ++entityCount;
    }

    EXPECT_EQ(entityCount, 1);
}
