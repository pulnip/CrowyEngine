#pragma once

#include <string>
#include <variant>
#include <vector>
#include "Keyboard.hpp"

namespace Crowy
{
    struct KeyboardBinding{
        KeyCode keyCode;
        KeyState keyState;
    };

    using Action = std::string;
    using InputBinding = std::variant<KeyboardBinding>;
    using InputBindings = std::vector<InputBinding>;

    struct ActionSpec{
        Action name;
        InputBindings bindings;
    };

    struct InputSpec{
        std::vector<ActionSpec> actions;
    };
}