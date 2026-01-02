#include "Input.hpp"
#include "InputManager.hpp"
#include "InputSpec.hpp"

namespace Crowy
{
    InputManager* InputManager::instance = nullptr;

    void initInputModule(std::unique_ptr<InputProvider> provider){
        InputManager::instance = new InputManager(std::move(provider));
    }

    void deinitInputModule(){
        delete InputManager::instance;

        InputManager::instance = nullptr;
    }

    void loadInputConfig(const InputSpec&){
        // TODO. Need InputMap
    }

    void pollInput(){
        InputManager::singleton()->pollInput();
    }

    bool isDown(KeyCode keyCode){
        return InputManager::singleton()->isDown(keyCode);
    }

    bool isNone(KeyCode keyCode){
        return InputManager::singleton()->isNone(keyCode);
    }

    bool isPressed(KeyCode keyCode){
        return InputManager::singleton()->isPressed(keyCode);
    }

    bool isReleased(KeyCode keyCode){
        return InputManager::singleton()->isReleased(keyCode);
    }

    bool isHeld(KeyCode keyCode){
        return InputManager::singleton()->isHeld(keyCode);
    }

    bool isAction(std::string_view action){
        return InputManager::singleton()->isAction(action);
    }
}