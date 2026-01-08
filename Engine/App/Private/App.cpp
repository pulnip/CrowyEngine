#include "App.hpp"
#include "AppConfig.hpp"
#include "GameMainLoop.hpp"
#include "Input.hpp"
#include "OS.hpp"
#include "RenderParser.hpp"
#include "Resource.hpp"
#include "Script.hpp"
#include "SceneParser.hpp"
#include "SceneLoader.hpp"

namespace Crowy
{
    std::unique_ptr<MainLoop> App::mainLoop = nullptr;

    Error App::setup(const AppConfig& config){
        auto os = OS::singleton();

        initResourceModule(os->getDevice());
        initInputModule(os->getInputProvider());
        initScriptModule();

        auto  sceneSpec = parseSceneFromFile(config.sceneFile);
        auto renderSpec = parseRenderFromFile(config.renderFile);

        mainLoop = std::make_unique<GameMainLoop>(
            sceneSpec, renderSpec
        );
        if(mainLoop == nullptr)
            return Error::FAILED;

        OS::singleton()->setMainLoop(mainLoop.get());

        return Error::OK;
    }

    void App::cleanup(){
        deinitInputModule();
        deinitScriptModule();
        deinitResourceModule();
    }
}