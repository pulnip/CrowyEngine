#include <memory>
#include <gtest/gtest.h>
#include "Input.hpp"
#include "MockInputProvider.hpp"

using Crowy::MockInputProvider, Crowy::KeyCode, Crowy::pollInput;
using Crowy::isDown, Crowy::isAction;
using Crowy::isNone, Crowy::isPressed, Crowy::isReleased, Crowy::isHeld;

class InputModuleTest: public ::testing::Test{
protected:
    static MockInputProvider* provider;

    static void SetUpTestSuite(){
        auto provider = std::make_unique<MockInputProvider>();
        InputModuleTest::provider = provider.get();
        Crowy::initInputModule(std::move(provider));
    }

    void SetUp() override{
        provider->reset();
    }

    static void TearDownTestSuite(){
        Crowy::deinitInputModule();
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
