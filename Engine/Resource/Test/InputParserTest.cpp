#include <gtest/gtest.h>
#include "InputParser.hpp"

using namespace Crowy;

TEST(InputParser, ParseInput){
    std::string tomlText = R"(
        [[actions]]
        name = "jump"
            [[actions.bindings]]
            device = "keyboard"
            keyCode = "space"
            [[actions.bindings]]
            device = "gamepad"
            button = "a"
    )";
    auto inputSpec = parseInputFromString(tomlText);
}