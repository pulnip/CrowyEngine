#include "math.hpp"
#include "simple_collision.hpp"
#include "ComponentDefinitions.hpp"
#include "Context.hpp"
#include "EntityRegistry.hpp"
#include "ECSSystem.hpp"
#include "OS.hpp"
#include "Renderer.hpp"
#include "Script.hpp"
#include "ray.hpp"
#define CROWY_UI_CONTEXT UIContext
#include "UIRenderer.hpp"

namespace Crowy
{
    void PlayerSystem::update(EntityRegistry& registry, UpdateContext& ctx){
        for(auto [id, bit,
            transform,
            controller
        ]: registry.query<
            TransformComponent,
            CharacterController
        >()){
            Ray ray{
                .point = transform.position,
                .dir = -transform.up()
            };

            for(auto [id, bit,
                transform,
                boxCollider
            ]: registry.query<
                TransformComponent,
                BoxColliderComponent
            >()){
                RaycastHit hit;
                // TODO.
                // if(raycastBox(ray, hit)){

                // }
            }
        }
    }

    void ScriptSystem::start(EntityRegistry& registry){
        for(auto [id, bit,
            script
        ]: registry.query<
            ScriptComponent
        >()){
            startScript(script.handle);
        }
    }

    void ScriptSystem::update(EntityRegistry& registry, UpdateContext& ctx){
        for(auto [id, bit,
            script
        ]: registry.query<
            ScriptComponent
        >()){
            updateScript(script.handle, ctx.deltaTime);
        }
    }

    void ScriptSystem::finish(EntityRegistry& registry){
        for(auto [id, bit,
            script
        ]: registry.query<
            ScriptComponent
        >()){
            finishScript(script.handle);
        }
    }

    void RenderSystem::update(EntityRegistry& registry, UpdateContext& ctx){
        const auto screenWidth = OS_->getWidth();
        const auto screenHeight = OS_->getHeight();

        auto cmdList = OS_->getCommandList();
        auto swapchain = OS_->getSwapchain();

        for(auto [id, bit,
            transform,
            camera
        ]: registry.query<
            TransformComponent,
            CameraComponent
        >()){
            // (w, h) = (0, 0) for fullscreen
            auto width  = camera.viewport.width  < 1.0f ?
                screenWidth  : camera.viewport.width;
            auto height = camera.viewport.height < 1.0f ?
                screenHeight : camera.viewport.height;
            auto aspect = static_cast<float>(width) / height;

            auto view = view_mat(transform.position, transform.rotation);
            auto proj = camera.proj==Projection::PERSPECTIVE ?
                perspective( camera.fov, aspect, camera.nearPlane, camera.farPlane) :
                orthographic(width, height, camera.nearPlane, camera.farPlane);

            std::vector<RenderItem> renderItems;
            auto renderObjView = registry.query<
                TransformComponent,
                RenderObjectComponent
            >();
            renderItems.reserve(renderObjView.size());

            for(auto [id, bit,
                transform,
                renderObj
            ]: renderObjView){
                auto model = model_mat(
                    transform.position,
                    transform.rotation,
                    transform.scale
                );

                renderItems.push_back(RenderItem{
                    .mesh = renderObj.mesh,
                    .materials = renderObj.materialSet,
                    .world = model,
                    .type = renderObj.renderType
                });
            }

            renderer.render(
                *cmdList,
                RenderContext{
                    .renderItems = renderItems,
                    .view = view,
                    .proj = proj
                },
                swapchain
            );

            UIContext uiContext{
                .renderer = renderer
            };
            uiRenderer.render(
                "UI", ui, uiContext,
                *cmdList,
                swapchain
            );
        }
    }
}