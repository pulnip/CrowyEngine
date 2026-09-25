#include <chrono>
#include <ratio>
#include <thread>

#include <gtest/gtest.h>

#include "Timer.hpp"

using namespace Crowy;
using namespace std::chrono_literals;

namespace
{
    // App's fixed step: 1/60 s, rounded once to the clock's tick
    constexpr auto Step = std::chrono::round<Timer::Duration>(
        std::chrono::duration<i64, std::ratio<1, 60>>(1)
    );
    constexpr f64 StepSeconds = std::chrono::duration<f64>(Step).count();
    // well above the few microseconds between two calls, well below the
    // sleeps these tests take
    constexpr f64 NoTime = 0.05;
}

TEST(Timer, StepsAddExactlyHoweverLongTheyTook) {
    Timer timer;

    for(int i = 0; i < 60; ++i) {
        std::this_thread::sleep_for(1ms);
        timer.Step(Step);
    }

    EXPECT_EQ(Step, 16'666'667ns);
    EXPECT_EQ(timer.GetDeltaTime(), 0.016666667);
    // sixty integer steps, so what ping.elapsed shows after run {"frames": 60}
    EXPECT_EQ(timer.GetElapsedTime(), 1.00000002);
}

TEST(Timer, AFrameAfterAStepDoesNotCountTheTimeBeforeIt) {
    Timer timer;
    std::this_thread::sleep_for(100ms);
    timer.Step(Step);

    timer.NewFrame();

    EXPECT_LT(timer.GetDeltaTime(), NoTime);
    EXPECT_LT(timer.GetElapsedTime(), StepSeconds + NoTime);
}

TEST(Timer, AFrameAfterARebaseDoesNotCountTheTimeBeforeIt) {
    Timer timer;
    std::this_thread::sleep_for(100ms);
    timer.Rebase();

    timer.NewFrame();

    EXPECT_LT(timer.GetDeltaTime(), NoTime);
    EXPECT_LT(timer.GetElapsedTime(), NoTime);
}
