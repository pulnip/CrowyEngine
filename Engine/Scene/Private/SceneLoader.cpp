#include "ComponentDefinitions.hpp"
#include "EntityRegistry.hpp"
#include "Resource.hpp"
#include "SceneLoader.hpp"
#include "SceneSpec.hpp"

namespace Crowy
{
    static RenderObjectComponent loadComponent(const RenderObjectSpec& spec){
        auto renderType = std::hash<RenderType>()(spec.renderType);

        auto [meshHandle, materialSetHandle] = getOrLoad(
            ModelRequest{.uri = spec.uri}
        );

        return RenderObjectComponent{
            .mesh = meshHandle,
            .materialSet = materialSetHandle,
            .renderType = renderType
        };
    }

    void loadScene(
        const SceneSpec& scene,
        EntityRegistry& registry
    ){
        for(const auto& entitySpec: scene.entities){
            std::optional<TransformComponent>      transformComponent      = std::nullopt;
            std::optional<RenderObjectComponent>   renderObjectComponent   = std::nullopt;
            std::optional<RigidbodyComponent>      rigidbodyComponent      = std::nullopt;
            std::optional<BoxColliderComponent>    boxColliderComponent    = std::nullopt;
            std::optional<SphereColliderComponent> sphereColliderComponent = std::nullopt;
            std::optional<CameraComponent>         cameraComponent         = std::nullopt;
            std::optional<PlayerComponent>         playerComponent         = std::nullopt;
            std::optional<EditorComponent>         editorComponent         = std::nullopt;

        #define LOAD_COMPONENT(name) \
            if(entitySpec.name##Index != INVALID_COMPONENT) \
                name##Component = scene.name##Specs[entitySpec.name##Index];

            LOAD_COMPONENT(transform)
            if(entitySpec.renderObjectIndex != INVALID_COMPONENT)
                renderObjectComponent = loadComponent(
                    scene.renderObjectSpecs[entitySpec.renderObjectIndex]
                );
            LOAD_COMPONENT(rigidbody)
            LOAD_COMPONENT(boxCollider)
            LOAD_COMPONENT(sphereCollider)
            LOAD_COMPONENT(camera)
            LOAD_COMPONENT(player)
            LOAD_COMPONENT(editor)
        #undef LOAD_COMPONENT

            // TODO
            // later, check component mutual exclusion here

            registry.createEntity(
                transformComponent,
                renderObjectComponent,
                rigidbodyComponent,
                boxColliderComponent,
                sphereColliderComponent,
                cameraComponent,
                playerComponent,
                editorComponent
            );
        }
    }
}