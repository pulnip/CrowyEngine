#pragma once

#include <memory>
#include <string_view>
#include "InputProvider.hpp"

namespace Crowy
{
    struct InputSpec;

    void initInputModule(InputProvider*);
    void deinitInputModule();

    void loadInputConfig(const InputSpec&);
    void unloadInputConfig();

    void pollInput();

    // test key directly
    bool isDown(KeyCode);
    bool isNone(KeyCode);
    bool isPressed(KeyCode);
    bool isReleased(KeyCode);
    bool isHeld(KeyCode);
    // test Action binding
    bool isAction(std::string_view);
}