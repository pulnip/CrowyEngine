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

        bool relative = false;
        // where the cursor hid, which it reports until it returns there
        Vec2 hiddenAt;

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

        // relative: the cursor hides where it is and the mouse reports motion
        // only; leaving puts it back there unless `restore` is false
        void SetRelativeMouse(SDL_Window* window, bool relative, bool restore = true) noexcept;
        bool IsRelativeMouse() const noexcept{
            return relative;
        }

        // Process Event
        void OnPlatformEvent(const SDL_KeyboardEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseMotionEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseButtonEvent& event) noexcept;
        void OnPlatformEvent(const SDL_MouseWheelEvent& event) noexcept;
    };
}
