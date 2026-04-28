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
        : renderer(*OS_->getDevice())
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

    void GameMainLoop::initialize(){
        // Game Logic Phase
        scheduler.attach(std::make_unique<ScriptSystem>());

        // Render Phase
        scheduler.attach(std::make_unique<RenderSystem>(
            renderer, ui, uiRenderer
        ));

        scheduler.start(registry);
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
        scheduler.finish(registry);

        unloadScriptConfig();
        unloadInputConfig();
    }
}