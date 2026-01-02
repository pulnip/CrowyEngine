#include "enum_traits.hpp"
#include "MockInputProvider.hpp"

namespace Crowy
{
    void MockInputProvider::poll(){
        previousKeys = currentKeys;
    }

    bool MockInputProvider::isKeyDown(KeyCode keyCode) const{
        auto ordKey = static_cast<size_t>(keyCode);
        return currentKeys.test(ordKey);
    }

    KeyState MockInputProvider::getKeyState(KeyCode keyCode) const{
        auto ordKey = static_cast<size_t>(keyCode);

        auto currentKeyState  = currentKeys.test(ordKey) ?
            KeyState::Pressed  : KeyState::None;
        auto previousKeyState = previousKeys.test(ordKey) ?
            KeyState::Released : KeyState::None;

        return combine(currentKeyState, previousKeyState);
    }

    void MockInputProvider::reset(){
        currentKeys.reset();
        previousKeys.reset();
    }

    void MockInputProvider::pressKey(KeyCode keyCode){
        auto ordKey = static_cast<size_t>(keyCode);
        currentKeys.set(ordKey);
    }

    void MockInputProvider::releaseKey(KeyCode keyCode){
        auto ordKey = static_cast<size_t>(keyCode);
        currentKeys.reset(ordKey);
    }
}