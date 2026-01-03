#pragma once

#include "semantics.hpp"
#include "Keyboard.hpp"

namespace Crowy
{
    class InputProvider{
    public:
        DECLARE_INTERFACE(InputProvider)

        virtual void poll() = 0;
        // only check current state
        virtual bool isKeyDown(KeyCode) const = 0;
        // combination of previous state and current state
        virtual KeyState getKeyState(KeyCode) const = 0;
    };
}