#include "MockInputProvider.hpp"

namespace Crowy
{
    void MockInputProvider::poll(){
        previousKeys = currentKeys;
        currentKeys = transitionKeys;
    }

    void MockInputProvider::reset(){
        transitionKeys.reset();
        currentKeys.reset();
        previousKeys.reset();
    }

    void MockInputProvider::pressKey(KeyCode keyCode){
        auto ordKey = static_cast<size_t>(keyCode);
        transitionKeys.set(ordKey);
    }

    void MockInputProvider::releaseKey(KeyCode keyCode){
        auto ordKey = static_cast<size_t>(keyCode);
        transitionKeys.reset(ordKey);
    }
}