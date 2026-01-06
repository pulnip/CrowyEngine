#include <memory>
#include <gtest/gtest.h>
#include "Input.hpp"
#include "InputSpec.hpp"
#include "MockInputProvider.hpp"

using namespace Crowy;

class InputModuleTest: public ::testing::Test{
protected:
    static std::unique_ptr<MockInputProvider> provider;

    static void SetUpTestSuite(){
        provider = std::make_unique<MockInputProvider>();
        initInputModule(provider.get());
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

std::unique_ptr<MockInputProvider> InputModuleTest::provider = nullptr;

TEST_F(InputModuleTest, KeyStateLifecycle){
    {
        SCOPED_TRACE("Frame 0");
        pollInput();

        // input occured, but applied at next frame.
        provider->pressKey(KeyCode::A);

        EXPECT_FALSE(isDown(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 1");
        pollInput();

        EXPECT_TRUE(isDown(KeyCode::A));
        EXPECT_FALSE(isNone(KeyCode::A));
        EXPECT_TRUE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 2");
        pollInput();

        provider->releaseKey(KeyCode::A);

        EXPECT_TRUE(isDown(KeyCode::A));
        EXPECT_FALSE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_FALSE(isReleased(KeyCode::A));
        EXPECT_TRUE(isHeld(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 3");
        pollInput();

        EXPECT_FALSE(isDown(KeyCode::A));
        EXPECT_FALSE(isNone(KeyCode::A));
        EXPECT_FALSE(isPressed(KeyCode::A));
        EXPECT_TRUE(isReleased(KeyCode::A));
        EXPECT_FALSE(isHeld(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 4");
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
        SCOPED_TRACE("Frame 0");
        pollInput();

        provider->pressKey(KeyCode::A);

        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::B));
        EXPECT_FALSE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 1");
        pollInput();

        provider->pressKey(KeyCode::B);

        EXPECT_TRUE(isPressed(KeyCode::A));
        EXPECT_TRUE(isNone(KeyCode::B));
        // A is pressed
        EXPECT_TRUE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 2");
        pollInput();

        provider->releaseKey(KeyCode::A);

        EXPECT_TRUE(isHeld(KeyCode::A));
        EXPECT_TRUE(isPressed(KeyCode::B));
        EXPECT_FALSE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 3");
        pollInput();

        provider->releaseKey(KeyCode::B);

        EXPECT_TRUE(isReleased(KeyCode::A));
        EXPECT_TRUE(isHeld(KeyCode::B));
        EXPECT_FALSE(isAction("testAction"));
    }

    {
        SCOPED_TRACE("Frame 4");
        pollInput();

        EXPECT_TRUE(isNone(KeyCode::A));
        EXPECT_TRUE(isReleased(KeyCode::B));
        // B is released
        EXPECT_TRUE(isAction("testAction"));
    }
}
