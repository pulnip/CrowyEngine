#pragma once

#include <SDL3/SDL_events.h>
#include "FastPimpl.hpp"
#include "Primitives.hpp"
#include "Window.hpp"

namespace Crowy
{
    class SDLWindow: public Window{
    private:
        class Impl;
        static constexpr usize implSize = 8;
        static constexpr usize implAlign = 8;
        FastPimpl<Impl, implSize, implAlign> impl;

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
