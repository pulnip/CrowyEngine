#pragma once

#include <bitset>
#include "InputProvider.hpp"

namespace Crowy
{
    class SDLInputProvider: public InputProvider{
    private:
        const bool* currentKeys;
        std::bitset<NUM_KEY> previousKeys;

    public:
        SDLInputProvider();

        void poll() override;
        bool isKeyDown(KeyCode) const override;
        KeyState getKeyState(KeyCode) const override;
    };
}