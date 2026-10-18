#pragma once

#include <SDL3/SDL_events.h>
#include "InputProvider.hpp"

namespace Crowy
{
    // what the UI took at its last frame: its events and held state read as up
    struct InputGate{
        bool mouse = false;
        bool keyboard = false;
    };

    class SDLInputProvider final: public InputProvider{
    private:
        const bool* heldState;

        std::bitset<NUM_MOUSE_BUTTON> mouseHeldState;
        InputGate gate;

    public:
        SDLInputProvider();
        ~SDLInputProvider();

        bool IsKeyDown(KeyCode) const noexcept override;

        bool IsKeyDown(MouseButton) const noexcept override;

        // Call Before Event Process
        void NewFrame(InputGate gate = {}) noexcept;

        void Reset() noexcept{
            InputProvider::ConsumeEdge();

            mouseHeldState.reset();
        }

        // Process Event
        void OnPlatformEvent(const SDL_KeyboardEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseButtonEvent& event) noexcept;
    };
}
