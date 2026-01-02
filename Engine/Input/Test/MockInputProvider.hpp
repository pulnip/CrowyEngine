#pragma once

#include <bitset>
#include "semantics.hpp"
#include "InputProvider.hpp"

namespace Crowy
{
    class MockInputProvider: public InputProvider{
    private:
        std::bitset<NUM_KEY> currentKeys;
        std::bitset<NUM_KEY> previousKeys;

    public:
        MockInputProvider() = default;
        ~MockInputProvider() = default;

        void poll() override;
        bool isKeyDown(KeyCode) const override;
        KeyState getKeyState(KeyCode) const override;

        void reset();
        void pressKey(KeyCode);
        void releaseKey(KeyCode);
    };
}