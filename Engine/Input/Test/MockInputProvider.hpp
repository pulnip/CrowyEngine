#pragma once

#include "semantics.hpp"
#include "InputProvider.hpp"

namespace Crowy
{
    class MockInputProvider: public InputProvider{
    private:
        std::bitset<NUM_KEY> transitionKeys;

    public:
        MockInputProvider() = default;
        ~MockInputProvider() = default;

        void poll() override;

        void reset();
        void pressKey(KeyCode);
        void releaseKey(KeyCode);
    };
}