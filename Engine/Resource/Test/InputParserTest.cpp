#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "type_traits.hpp"
#include "InputParser.hpp"

using ::testing::Field;
using ::testing::SizeIs;
using ::testing::Eq;

using namespace Crowy;

TEST(InputParser, ParseInput){
    std::string tomlText = R"(
        [[actions]]
        name = "action1"
            [[actions.bindings]]
            device = "keyboard"
            keyCode = "space"
            keyState = "Pressed"
            [[actions.bindings]]
            device = "keyboard"
            keyCode = "a"
            keyState = "Released"
            [[actions.bindings]]
            device = "gamepad"
            button = "a"
        [[actions]]
        name = "action2"
            [[actions.bindings]]
            device = "keyboard"
            keyCode = "ctrl"
            keyState = "None"
    )";
    auto inputSpec = parseInputFromString(tomlText);

    ASSERT_THAT(inputSpec.actions, SizeIs(2));

    EXPECT_THAT(inputSpec.actions, Contains(AllOf(
        Field(&ActionSpec::name, Eq("action1")),
        Field(&ActionSpec::bindings, UnorderedElementsAre(
            // not support gamepad for now.
            VariantWith<KeyboardBinding>(AllOf(
                Field(&KeyboardBinding::keyCode, Eq(KeyCode::Space)),
                Field(&KeyboardBinding::keyState, Eq(KeyState::Pressed))
            )),
            VariantWith<KeyboardBinding>(AllOf(
                Field(&KeyboardBinding::keyCode, Eq(KeyCode::A)),
                Field(&KeyboardBinding::keyState, Eq(KeyState::Released))
            ))
        ))
    )));
    EXPECT_THAT(inputSpec.actions, Contains(AllOf(
        Field(&ActionSpec::name, Eq("action2")),
        Field(&ActionSpec::bindings, UnorderedElementsAre(
            VariantWith<KeyboardBinding>(AllOf(
                Field(&KeyboardBinding::keyCode, Eq(KeyCode::Ctrl)),
                Field(&KeyboardBinding::keyState, Eq(KeyState::None))
            ))
        ))
    )));
}