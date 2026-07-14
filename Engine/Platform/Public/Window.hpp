#pragma once

#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct WindowConfig;

    class Window{
    public:
        CROWY_DECLARE_INTERFACE(Window)

        virtual void* GetWindow() const noexcept = 0;
        virtual u32 GetWidth() const noexcept = 0;
        virtual u32 GetHeight() const noexcept = 0;
        virtual Size2D GetSize() const noexcept = 0;
    };
}
