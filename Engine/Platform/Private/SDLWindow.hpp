#pragma once

#include <SDL3/SDL_events.h>
#include "Primitives.hpp"
#include "Window.hpp"

namespace Crowy
{
    class SDLWindow: public Window{
    private:
        class Impl;
        RAII<Impl> impl;

    public:
        SDLWindow(const WindowConfig&);
        ~SDLWindow();
        CROWY_DECLARE_PINNED(SDLWindow)

        void* GetWindow() const noexcept;
        u32 GetWidth() const noexcept;
        u32 GetHeight() const noexcept;
        Size2D GetSize() const noexcept;

        void OnPlatformEvent(const SDL_WindowEvent&) noexcept;
    };
}
