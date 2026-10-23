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

    // Held keys and buttons and the mouse position come from the events, as
    // every other state does, so an event made by hand counts as a real one.
    class SDLInputProvider final: public InputProvider{
    private:
        std::bitset<NUM_KEY> keyHeldState;
        std::bitset<NUM_MOUSE_BUTTON> mouseHeldState;
        InputGate gate;

        // the position stays where the cursor hid until it returns there
        bool relative = false;

    public:
        SDLInputProvider();
        ~SDLInputProvider();

        bool IsKeyDown(KeyCode) const noexcept override;

        bool IsKeyDown(MouseButton) const noexcept override;

        // Call Before Event Process
        void NewFrame(InputGate gate = {}) noexcept;

        void Reset() noexcept{
            InputProvider::ConsumeEdge();

            keyHeldState.reset();
            mouseHeldState.reset();
        }

        // relative: the cursor hides where it is and the mouse reports motion
        // only; leaving puts it back there unless `restore` is false
        void SetRelativeMouse(SDL_Window* window, bool relative, bool restore = true) noexcept;
        bool IsRelativeMouse() const noexcept{
            return relative;
        }
        // keys get their releases from SDL; buttons get theirs only once the
        // window is focused again, so they let go here
        void OnFocusLost(SDL_Window* window) noexcept;

        // Process Event
        void OnPlatformEvent(const SDL_KeyboardEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseMotionEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseButtonEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseWheelEvent& event) noexcept;
    };
}
