#include "enum_traits.hpp"
#include "InputProvider.hpp"

namespace Crowy
{
    bool InputProvider::isKeyDown(KeyCode keyCode) const{
        auto ordKey = static_cast<size_t>(keyCode);
        return currentKeys.test(ordKey);
    }

    KeyState InputProvider::getKeyState(KeyCode keyCode) const{
        auto ordKey = static_cast<size_t>(keyCode);

        auto currentKeyState  = currentKeys.test(ordKey) ?
            KeyState::Pressed  : KeyState::None;
        auto previousKeyState = previousKeys.test(ordKey) ?
            KeyState::Released : KeyState::None;

        return combine(currentKeyState, previousKeyState);
    }
}