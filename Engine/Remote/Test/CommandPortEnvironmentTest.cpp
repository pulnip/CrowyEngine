#include <cstdlib>

#include <gtest/gtest.h>

#include "CommandPort.hpp"

using namespace Crowy;

namespace
{
    void setPortVariable(const char* value) {
#if defined(_WIN32)
        _putenv_s("CROWY_COMMAND_PORT", value != nullptr ? value : "");
#else
        if(value != nullptr)
            setenv("CROWY_COMMAND_PORT", value, 1);
        else
            unsetenv("CROWY_COMMAND_PORT");
#endif
    }

    class CommandPortEnvironmentTest: public testing::Test {
    protected:
        void TearDown() override { setPortVariable(nullptr); }
    };
}

TEST_F(CommandPortEnvironmentTest, UnsetIsTheDefaultPortWithRetries) {
    setPortVariable(nullptr);

    const auto config = commandPortConfigFromEnvironment();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->port, CommandPortConfig{}.port);
    EXPECT_EQ(config->portRetries, CommandPortConfig{}.portRetries);
}

TEST_F(CommandPortEnvironmentTest, ZeroDisablesThePort) {
    setPortVariable("0");

    EXPECT_FALSE(commandPortConfigFromEnvironment());
}

TEST_F(CommandPortEnvironmentTest, ANumberForcesExactlyThatPort) {
    setPortVariable("27533");

    const auto config = commandPortConfigFromEnvironment();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->port, 27533);
    EXPECT_EQ(config->portRetries, 0);
}

TEST_F(CommandPortEnvironmentTest, GarbageFallsBackToTheDefault) {
    setPortVariable("not-a-port");

    const auto config = commandPortConfigFromEnvironment();
    ASSERT_TRUE(config);
    EXPECT_EQ(config->port, CommandPortConfig{}.port);
    EXPECT_EQ(config->portRetries, CommandPortConfig{}.portRetries);
}
