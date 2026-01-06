#include "App.hpp"
#include "AppConfig.hpp"
#include "GameMainLoop.hpp"
#include "Input.hpp"
#include "OS.hpp"
#include "Resource.hpp"
#include "Script.hpp"

namespace Crowy
{
    std::unique_ptr<MainLoop> App::mainLoop = nullptr;

    Error App::setup(int argc, char* argv[]){
        auto config = parseCommandLine(argc, argv);
        auto os = OS::singleton();

        initResourceModule(os->getDevice());
        initInputModule(os->getInputProvider());
        initScriptModule();

        mainLoop = std::make_unique<GameMainLoop>();
        if(!mainLoop)
            return Error::FAILED;

        return Error::OK;
    }

    bool App::start(){
        OS::singleton()->setMainLoop(mainLoop.get());

        return true;
    }

    void App::cleanup(){
        deinitInputModule();
        deinitScriptModule();
        deinitResourceModule();
    }
}