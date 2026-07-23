#pragma once

#include <bitset>
#include "InputProvider.hpp"
#include "KeyCode.hpp"

namespace Crowy
{
    // Does not handle mouse for now
    class MockInputProvider final: public InputProvider{
    private:
        std::bitset<NUM_KEY> heldState;
        std::bitset<NUM_KEY> pressedState;
        std::bitset<NUM_KEY> releasedState;

    public:
        MockInputProvider() = default;
        ~MockInputProvider() = default;

        bool IsKeyDown(KeyCode keyCode) const noexcept override{
            auto ordKey = static_cast<usize>(keyCode);

            return heldState.test(ordKey);
        }

        bool IsKeyDown(MouseButton) const noexcept override{
            return false;
        }

        void NewFrame() noexcept{
            InputProvider::ConsumeEdge();
        }
        void Reset() noexcept{
            InputProvider::ConsumeEdge();
            heldState.reset();
        }

        // simulate keyboard
        void PressKey(KeyCode keyCode) noexcept{
            InputProvider::PressKey(keyCode);
            auto ordKey = static_cast<usize>(keyCode);

            heldState.set(ordKey, true);
        }
        void ReleaseKey(KeyCode keyCode) noexcept{
            InputProvider::ReleaseKey(keyCode);
            auto ordKey = static_cast<usize>(keyCode);

            heldState.set(ordKey, false);
        }
    };
}
