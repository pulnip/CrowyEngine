#pragma once

#include <SDL3/SDL_video.h>

#include "Semantics.hpp"

namespace Crowy
{
    struct WindowConfig;

    class SDLWindow {
    private:
        SDL_Window* window = nullptr;

    public:
        ~SDLWindow();
        CROWY_DECLARE_PINNED(SDLWindow)

        explicit SDLWindow(const WindowConfig&);

        SDL_Window* Handle() const noexcept { return window; }
    };
}
