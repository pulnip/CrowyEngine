#include "App.hpp"
#include "AppConfig.hpp"
#include "ConfigParser.hpp"
#include "GameMainLoop.hpp"
#include "Input.hpp"
#include "OS.hpp"
#include "Resource.hpp"
#include "Script.hpp"

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
        // auto scriptSpec = parseScriptFromFile(config.scriptFile);
        ScriptSpec scriptSpec;

        mainLoop = std::make_unique<GameMainLoop>(
            sceneSpec, renderSpec, scriptSpec
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