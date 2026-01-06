#include "App.hpp"
#include "OS.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    OS os;

    if(App::setup(argc, argv) != Error::OK)
        return 1;

    if(App::start())
        os.run();

    App::cleanup();
    return os.getExitCode();
}