#pragma once

#include <variant>

#include "KeyCode.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    struct KeyInput {
        KeyCode key = KeyCode::Unknown;
        bool down = false;
    };

    struct ButtonInput {
        MouseButton button = MouseButton::Unknown;
        bool down = false;
    };

    // by window points, as a device's relative motion
    struct MoveInput {
        Vec2 by{0.0f, 0.0f};
    };

    // to a window point, from the top-left
    struct MoveToInput {
        Vec2 at{0.0f, 0.0f};
    };

    // notches, positive away from the user
    struct WheelInput {
        f32 notches = 0.0f;
    };

    // An event made by hand, which a --port-input run takes in place of its
    // devices; the OS runs it down the path a device's event takes.
    using InputEvent =
        std::variant<KeyInput, ButtonInput, MoveInput, MoveToInput, WheelInput>;

    struct InputStatus {
        // a --port-input run's is only logical: the OS cursor is left alone
        bool relativeMouse = false;
        // what the UI took at its last frame, hidden from the app in this one
        bool uiHasMouse = false;
        bool uiHasKeyboard = false;
    };
}
