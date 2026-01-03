#pragma once

#include <bitset>
#include "semantics.hpp"
#include "Keyboard.hpp"

namespace Crowy
{
    class InputProvider{
    protected:
        std::bitset<NUM_KEY> currentKeys;
        std::bitset<NUM_KEY> previousKeys;

    public:
        DECLARE_INTERFACE(InputProvider)

        virtual void poll() = 0;
        // only check current state
        virtual bool isKeyDown(KeyCode keyCode) const;
        // combination of previous state and current state
        virtual KeyState getKeyState(KeyCode) const;
    };
}