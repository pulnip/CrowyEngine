#include "ECSSystem.hpp"
#include "GameMainLoop.hpp"
#include "OS.hpp"
#include "SceneLoader.hpp"

namespace Crowy
{
    GameMainLoop::GameMainLoop(
        const SceneSpec& sceneSpec,
        const RenderSpec& renderSpec
    )
        : renderer(OS_->getDevice())
        , uiRenderer(OS_->getWindow(), *OS_->getDevice())
    {
        loadScene(sceneSpec, registry);

        renderer.loadPasses(renderSpec,
            OS_->getWidth(),
            OS_->getHeight()
        );
    }

    void GameMainLoop::initialize(){
        // Game Logic Phase

        // Render Phase
        scheduler.attach(std::make_unique<RenderSystem>(
            renderer, ui, uiRenderer
        ));
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