#pragma once

#include "Primitives.hpp"

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
        // Arrows
        Left, Right, Up, Down,
        // Sentinel
        Unknown
    };
    constexpr auto NUM_KEY = static_cast<usize>(KeyCode::Unknown);

    enum class MouseButton{
        LButton, RButton,
        // Sentinel
        Unknown
    };
    constexpr auto NUM_MOUSE_BUTTON = static_cast<usize>(MouseButton::Unknown);
}
