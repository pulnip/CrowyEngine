#include <format>
#include "InputManager.hpp"

namespace Crowy
{
    void InputManager::loadConfig(const InputSpec& spec){
        for(const auto& action: spec.actions){
            actionMap[action.name] = action.bindings;
        }
    }

    void InputManager::unloadConfig(){
        actionMap.clear();
    }

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
        return keyState == KeyState::None;
    }

    bool InputManager::isPressed(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return keyState == KeyState::Pressed;
    }

    bool InputManager::isReleased(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return keyState == KeyState::Released;
    }

    bool InputManager::isHeld(KeyCode keyCode) const{
        auto keyState = getKeyState(keyCode);
        return keyState == KeyState::Held;
    }

    bool InputManager::isAction(std::string_view action) const{
        auto it = actionMap.find(action);
        if(it == actionMap.end())
            throw std::runtime_error(std::format(
                "Unregistered action: {}", action
            ));

        struct InputBindingVisitor{
            const InputManager& manager;

            bool operator()(const KeyboardBinding& source) const{
                return manager.getKeyState(source.keyCode) == source.keyState;
            }
        };

        for(const auto& source: it->second){
            if(std::visit(InputBindingVisitor{.manager = *this}, source))
                return true;
        }

        return false;
    }
}