#include "App.hpp"
#include "AppConfig.hpp"
#include "OS.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    auto config = parseCommandLine(argc, argv);

    OS os(config.window);

    if(App::setup(config) == Error::OK)
        os.run();

    App::cleanup();
    return os.getExitCode();
}