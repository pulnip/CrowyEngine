#include "ECSSystem.hpp"
#include "GameMainLoop.hpp"
#include "Input.hpp"
#include "OS.hpp"
#include "SceneLoader.hpp"
#include "Script.hpp"

namespace Crowy
{
    GameMainLoop::GameMainLoop(
        const RenderSpec& renderSpec,
        const InputSpec&  inputSpec,
        const ScriptSpec& scriptSpec,
        const SceneSpec& sceneSpec
    )
        : renderer(OS_->getDevice())
        , uiRenderer(OS_->getWindow(), *OS_->getDevice())
    {
        renderer.loadPasses(renderSpec,
            OS_->getWidth(),
            OS_->getHeight()
        );

        loadInputConfig(inputSpec);
        loadScriptConfig(scriptSpec);
        loadScene(sceneSpec, registry);
    }

    GameMainLoop::~GameMainLoop(){
        unloadScriptConfig();
        unloadInputConfig();
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