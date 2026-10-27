#include <bitset>

#include <gtest/gtest.h>

#include "InputProvider.hpp"
#include "KeyCode.hpp"

using namespace Crowy;

namespace
{
    class FakeInputProvider final: public InputProvider {
    private:
        std::bitset<NUM_KEY> heldState;

    public:
        bool IsKeyDown(KeyCode keyCode) const noexcept override {
            return heldState.test(static_cast<usize>(keyCode));
        }

        bool IsKeyDown(MouseButton) const noexcept override { return false; }

        void NewFrame() noexcept { InputProvider::ConsumeEdge(); }
        void Reset() noexcept {
            InputProvider::ConsumeEdge();
            heldState.reset();
        }

        void PressKey(KeyCode keyCode) noexcept {
            InputProvider::PressKey(keyCode);
            heldState.set(static_cast<usize>(keyCode), true);
        }
        void ReleaseKey(KeyCode keyCode) noexcept {
            InputProvider::ReleaseKey(keyCode);
            heldState.set(static_cast<usize>(keyCode), false);
        }
    };
}

class InputProviderTest: public ::testing::Test {
protected:
    static FakeInputProvider provider;

    void SetUp() override { provider.Reset(); }
};

FakeInputProvider InputProviderTest::provider;

TEST_F(InputProviderTest, KeyStateLifecycleOccurFirst) {
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
