#include <memory>
#include <gtest/gtest.h>
#include "Input.hpp"
#include "InputSpec.hpp"
#include "MockInputProvider.hpp"

using namespace Crowy;

class InputModuleTest: public ::testing::Test{
protected:
    static MockInputProvider* provider;

    static void SetUpTestSuite(){
        auto provider = std::make_unique<MockInputProvider>();
        InputModuleTest::provider = provider.get();
        initInputModule(std::move(provider));
    }

    void SetUp() override{
        provider->reset();
    }

    void TearDown() override{
        unloadInputConfig();
    }

    static void TearDownTestSuite(){
        deinitInputModule();
    }
};

MockInputProvider* InputModuleTest::provider = nullptr;

TEST_F(InputModuleTest, KeyStateLifecycle){
    {
        SCOPED_TRACE("Frame 0");
        pollInput();

        EXPECT_FALSE(isDown(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));

        provider->pressKey(KeyCode::A);

        EXPECT_TRUE(isDown(KeyCode::A));
        EXPECT_FALSE(isNone(KeyCode::A));
        EXPECT_TRUE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 1");
        pollInput();

        EXPECT_TRUE(isDown(KeyCode::A));
        EXPECT_FALSE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_TRUE(isHeld(KeyCode::A));

        provider->releaseKey(KeyCode::A);

        EXPECT_FALSE(isDown(KeyCode::A));
        EXPECT_FALSE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_TRUE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 2");
        pollInput();

        EXPECT_FALSE(isDown(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));
    }
}

TEST_F(InputModuleTest, InputAction){
    InputSpec spec{
        .actions = {{
            .name = "testAction",
            .bindings = {
                KeyboardBinding{
                    .keyCode = KeyCode::A,
                    .keyState = KeyState::Pressed
                },
                KeyboardBinding{
                    .keyCode = KeyCode::B,
                    .keyState = KeyState::Released
                }
            }
        }}
    };

    loadInputConfig(spec);

    {
        SCOPED_TRACE("Frame 1");
        pollInput();

        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::B));
        EXPECT_FALSE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 2");
        pollInput();

        provider->pressKey(KeyCode::A);

        EXPECT_TRUE(isPressed(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::B));
        // A is pressed
        EXPECT_TRUE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 3");
        pollInput();

        provider->pressKey(KeyCode::B);

        EXPECT_TRUE(isHeld(KeyCode::A));
        EXPECT_TRUE(isPressed(KeyCode::B));
        EXPECT_FALSE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 4");
        pollInput();

        provider->releaseKey(KeyCode::A);

        EXPECT_TRUE(isReleased(KeyCode::A));
        EXPECT_TRUE(isHeld(KeyCode::B));
        EXPECT_FALSE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 5");
        pollInput();

        provider->releaseKey(KeyCode::B);

        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_TRUE(isReleased(KeyCode::B));
        // B is released
        EXPECT_TRUE(isAction("testAction"));
    }
}
