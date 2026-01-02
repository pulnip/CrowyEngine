#include <format>
#include "enum_traits.hpp"
#include "InputManager.hpp"

namespace Crowy
{
    void InputManager::pollInput(){
        provider->poll();
    }

    KeyState InputManager::getKeyState(KeyCode keyCode) const{
        return provider->getKeyState(keyCode);
    }

    bool InputManager::isDown(KeyCode keyCode) const{
        return provider->isKeyDown(keyCode);
    }

    bool InputManager::isNone(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return hasFlag(keyState, KeyState::None);
    }

    bool InputManager::isPressed(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return hasFlag(keyState, KeyState::Pressed);
    }

    bool InputManager::isReleased(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return hasFlag(keyState, KeyState::Released);
    }

    bool InputManager::isHeld(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return hasFlag(keyState, KeyState::Held);
    }

    bool InputManager::isAction(std::string_view action) const{
        auto it = actionMap.find(action);
        if(it == actionMap.end())
            throw std::runtime_error(std::format(
                "Unregistered action: {}", action
            ));

        for(const auto& source: it->second){
            if(getKeyState(source.keyCode) == source.keyState)
                return true;
        }

        return false;
    }
}