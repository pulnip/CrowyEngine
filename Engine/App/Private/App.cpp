#include "App.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"

namespace Crowy
{
    auto App::mainLoop = std::make_unique<MainLoop>();

    App::App(){}

    App::~App(){}

    Error App::setup(int argc, char* argv[]){
        return Error::OK;
    }

    bool App::start(){
        OS::get()->setMainLoop(mainLoop.get());

        return true;
    }

    void App::cleanup(){

    }
}