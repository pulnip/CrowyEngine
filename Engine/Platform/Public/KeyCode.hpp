#pragma once

#include "EnumUtil.hpp"
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
        LButton, RButton, MButton,
        // Sentinel
        Unknown
    };
    constexpr auto NUM_MOUSE_BUTTON = static_cast<usize>(MouseButton::Unknown);

    // clang-format off
    CROWY_ENUM_BEGIN(KeyCode)
        CROWY_ENUM_VALUE(Num0)
        CROWY_ENUM_VALUE(Num1)
        CROWY_ENUM_VALUE(Num2)
        CROWY_ENUM_VALUE(Num3)
        CROWY_ENUM_VALUE(Num4)
        CROWY_ENUM_VALUE(Num5)
        CROWY_ENUM_VALUE(Num6)
        CROWY_ENUM_VALUE(Num7)
        CROWY_ENUM_VALUE(Num8)
        CROWY_ENUM_VALUE(Num9)
        CROWY_ENUM_VALUE(A)
        CROWY_ENUM_VALUE(B)
        CROWY_ENUM_VALUE(C)
        CROWY_ENUM_VALUE(D)
        CROWY_ENUM_VALUE(E)
        CROWY_ENUM_VALUE(F)
        CROWY_ENUM_VALUE(G)
        CROWY_ENUM_VALUE(H)
        CROWY_ENUM_VALUE(I)
        CROWY_ENUM_VALUE(J)
        CROWY_ENUM_VALUE(K)
        CROWY_ENUM_VALUE(L)
        CROWY_ENUM_VALUE(M)
        CROWY_ENUM_VALUE(N)
        CROWY_ENUM_VALUE(O)
        CROWY_ENUM_VALUE(P)
        CROWY_ENUM_VALUE(Q)
        CROWY_ENUM_VALUE(R)
        CROWY_ENUM_VALUE(S)
        CROWY_ENUM_VALUE(T)
        CROWY_ENUM_VALUE(U)
        CROWY_ENUM_VALUE(V)
        CROWY_ENUM_VALUE(W)
        CROWY_ENUM_VALUE(X)
        CROWY_ENUM_VALUE(Y)
        CROWY_ENUM_VALUE(Z)
        CROWY_ENUM_VALUE(F1)
        CROWY_ENUM_VALUE(F2)
        CROWY_ENUM_VALUE(F3)
        CROWY_ENUM_VALUE(F4)
        CROWY_ENUM_VALUE(F5)
        CROWY_ENUM_VALUE(F6)
        CROWY_ENUM_VALUE(F7)
        CROWY_ENUM_VALUE(F8)
        CROWY_ENUM_VALUE(F9)
        CROWY_ENUM_VALUE(F10)
        CROWY_ENUM_VALUE(F11)
        CROWY_ENUM_VALUE(F12)
        CROWY_ENUM_VALUE(Ctrl)
        CROWY_ENUM_VALUE(Alt)
        CROWY_ENUM_VALUE(Shift)
        CROWY_ENUM_VALUE(Tab)
        CROWY_ENUM_VALUE(Space)
        CROWY_ENUM_VALUE(Enter)
        CROWY_ENUM_VALUE(Escape)
        CROWY_ENUM_VALUE(Left)
        CROWY_ENUM_VALUE(Right)
        CROWY_ENUM_VALUE(Up)
        CROWY_ENUM_VALUE(Down)
    CROWY_ENUM_END()

    CROWY_ENUM_BEGIN(MouseButton)
        CROWY_ENUM_VALUE(LButton)
        CROWY_ENUM_VALUE(RButton)
        CROWY_ENUM_VALUE(MButton)
    CROWY_ENUM_END()
    // clang-format on
}
