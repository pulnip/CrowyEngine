#include <gtest/gtest.h>
#include "KeyCode.hpp"
#include "MockInputProvider.hpp"

using namespace Crowy;

class InputProviderTest: public ::testing::Test{
protected:
    static MockInputProvider provider;

    void SetUp() override{
        provider.Reset();
    }
};

MockInputProvider InputProviderTest::provider;

TEST_F(InputProviderTest, KeyStateLifecycleOccurFirst){
    {
        SCOPED_TRACE("Frame 0");
        provider.NewFrame();

        EXPECT_FALSE(provider.IsKeyDown(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyPressed(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyReleased(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 1");
        provider.NewFrame();

        // handle KEY_DOWN event
        provider.PressKey(KeyCode::A);

        EXPECT_TRUE(provider.IsKeyDown(KeyCode::A));
        EXPECT_TRUE(provider.IsKeyPressed(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyReleased(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 2");
        provider.NewFrame();

        EXPECT_TRUE(provider.IsKeyDown(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyPressed(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyReleased(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 3");
        provider.NewFrame();

        // handle KEY_UP event
        provider.ReleaseKey(KeyCode::A);

        EXPECT_FALSE(provider.IsKeyDown(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyPressed(KeyCode::A));
        EXPECT_TRUE(provider.IsKeyReleased(KeyCode::A));
    }

    {
        SCOPED_TRACE("Frame 4");
        provider.NewFrame();

        EXPECT_FALSE(provider.IsKeyDown(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyPressed(KeyCode::A));
        EXPECT_FALSE(provider.IsKeyReleased(KeyCode::A));
    }
}
