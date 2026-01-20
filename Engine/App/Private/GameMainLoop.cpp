#include "math.hpp"
#include "ECSSystem.hpp"
#include "GameMainLoop.hpp"
#include "OS.hpp"
#include "SceneLoader.hpp"

namespace Crowy
{
    void attachDefaultECSSystems(ECSScheduler& scheduler){
        scheduler.attach(std::make_unique<RenderSystem>());
    }

    GameMainLoop::GameMainLoop(
        const SceneSpec& sceneSpec,
        const RenderSpec& renderSpec
    )
        :renderer(OS::singleton()->getDevice())
    {
        loadScene(sceneSpec, registry);

        renderer.loadPasses(renderSpec);
    }

    void GameMainLoop::initialize(){
        attachDefaultECSSystems(scheduler);
    }

    bool GameMainLoop::update(float deltaTime, float totalTime){
        // Game Logic Phase
        UpdateContext context{
            .deltaTime = deltaTime,
            .totalTime = totalTime
        };
        scheduler.update(registry, context);

        // Render Phase
        std::vector<RenderItem> renderItems;
        for(auto [id, bit, t, r]: registry.query<
            TransformComponent, RenderObjectComponent
        >()){
            auto tMat = translateMat(t.position);
            auto rMat =    rotateMat(t.rotation);
            auto sMat =     scaleMat(t.scale   );
            auto model = tMat*rMat*sMat;

            renderItems.push_back(RenderItem{
                .mesh = r.mesh,
                .materials = r.materialSet,
                .world = model,
                .type = r.renderType
            });
        }

        auto os = OS::singleton();
        auto screenWidth = os->getWidth(), screenHeight = os->getHeight();
        auto cmdList = os->getCommandList();

        for(auto [id, bit, t, c]: registry.query<
            TransformComponent, CameraComponent
        >()){
            // (w, h) = (0, 0) for fullscreen
            auto width  = c.viewport.width  < 1.0f ?
                screenWidth  : c.viewport.width;
            auto height = c.viewport.height < 1.0f ?
                screenHeight : c.viewport.height;
            auto aspect = static_cast<float>(width) / height;

            auto view = viewMat(t.position, t.rotation);
            auto proj = c.proj==Projection::PERSPECTIVE ?
                perspective( c.fov, aspect, c.nearPlane, c.farPlane) :
                orthographic(width, height, c.nearPlane, c.farPlane);

            renderer.render(
                *cmdList,
                RenderContext{
                    .renderItems = renderItems,
                    .view = view,
                    .proj = proj,
                    .viewport = c.viewport
                },
                OS::singleton()->getSwapchain()
            );
        }
        return true;
    }

    void GameMainLoop::finalize(){

    }
}