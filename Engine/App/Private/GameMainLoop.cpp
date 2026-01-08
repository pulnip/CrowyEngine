#include "ECSSystem.hpp"
#include "GameMainLoop.hpp"
#include "OS.hpp"
#include "Resource.hpp"
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
        UpdateContext context{
            .deltaTime = deltaTime,
            .totalTime = totalTime
        };
        scheduler.update(registry, context);



        return true;
    }

    void GameMainLoop::finalize(){

    }
}