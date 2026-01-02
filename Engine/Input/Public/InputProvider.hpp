#pragma once

#include <cstddef>
#include "semantics.hpp"

namespace Crowy
{
    enum class KeyCode{
        // Numbers
        Num0, Num1, Num2, Num3, Num4,
        Num5, Num6, Num7, Num8, Num9,
        // Letters
        A, B, C, D, E, F, G, H, I, J,
        K, L, M, N, O, P, Q, R, S, T,
        U, V, W, X, Y, Z,
        // Function Keys
        F1, F2, F3,  F4,  F5,  F6,
        F7, F8, F9, F10, F11, F12,
        // Modifiers
        Ctrl, Alt, Shift,
        // Special
        Tab, Space, Enter, Escape,
        // Sentinel
        Unknown,
        Count
    };
    constexpr auto NUM_KEY = static_cast<size_t>(KeyCode::Count);

    enum class KeyState{
        None     = 0b00,
        Pressed  = 0b01,
        Released = 0b10,
        Held     = 0b11
    };

    class InputProvider{
    public:
        DECLARE_INTERFACE(InputProvider)

        virtual void poll() = 0;
        // only check current state
        virtual bool isKeyDown(KeyCode) const = 0;
        // combination of previous state and current state
        virtual KeyState getKeyState(KeyCode) const = 0;
    };
}