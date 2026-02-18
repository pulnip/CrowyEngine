#include "App.hpp"
#include "AppConfig.hpp"
#include "ConfigParser.hpp"
#include "GameMainLoop.hpp"
#include "Input.hpp"
#include "Logger.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "Resource.hpp"
#include "Script.hpp"

namespace Crowy
{
    std::unique_ptr<MainLoop> App::mainLoop = nullptr;

    Error App::setup(const AppConfig& config){
        Logger::instance().setMinLevel(LogLevel::Warn);

        initResourceModule(OS_->getDevice());
        initInputModule(OS_->getInputProvider());
        initScriptModule();

        auto renderSpec = parseRenderFromFile(config.renderFile);
        auto  inputSpec = parseInputFromFile(config.inputFile);
        auto scriptSpec = parseScriptFromFile(config.scriptFile);
        auto  sceneSpec = parseSceneFromFile(config.sceneFile);

        mainLoop = std::make_unique<GameMainLoop>(
            renderSpec, inputSpec, scriptSpec, sceneSpec
        );
        if(mainLoop == nullptr)
            return Error::FAILED;

        OS_->setMainLoop(mainLoop.get());

        return Error::OK;
    }

    void App::cleanup(){
        mainLoop.reset();

        deinitScriptModule();
        deinitInputModule();
        deinitResourceModule();
    }
}