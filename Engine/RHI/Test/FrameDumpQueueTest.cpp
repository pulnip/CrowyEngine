#include <atomic>
#include <chrono>
#include <memory>
#include <thread>

#include <gtest/gtest.h>

#include "FakeSwapchain.hpp"

using namespace Crowy;
using namespace std::chrono_literals;

TEST(FrameDumpQueue, RequestsComeDueInFrameOrder) {
    FakeSwapchain swapchain;
    swapchain.RequestFrameDump("b.bmp", 20);
    swapchain.RequestFrameDump("a.bmp", 10);

    EXPECT_FALSE(swapchain.TakeFrameDump(9).has_value());

    const auto first = swapchain.TakeFrameDump(10);
    ASSERT_TRUE(first.has_value());
    EXPECT_EQ(first->path, "a.bmp");
    EXPECT_FALSE(swapchain.TakeFrameDump(10).has_value());

    const auto second = swapchain.TakeFrameDump(20);
    ASSERT_TRUE(second.has_value());
    EXPECT_EQ(second->path, "b.bmp");

    // a backend always reports; one that never did would hold the destructor
    first->completion(true);
    second->completion(true);
}

TEST(FrameDumpQueue, AFrameOrAPathAlreadyQueuedIsReported) {
    FakeSwapchain swapchain;
    swapchain.RequestFrameDump("a.bmp", 10);

    EXPECT_TRUE(swapchain.IsFrameDumpQueued(10, "other.bmp"));
    EXPECT_TRUE(swapchain.IsFrameDumpQueued(11, "a.bmp"));
    EXPECT_FALSE(swapchain.IsFrameDumpQueued(11, "b.bmp"));

    // still being written, so the path is still taken
    auto job = swapchain.TakeFrameDump(10);
    ASSERT_TRUE(job.has_value());
    EXPECT_TRUE(swapchain.IsFrameDumpQueued(12, "a.bmp"));

    job->completion(true);
    EXPECT_TRUE(swapchain.IsFrameDumpQueued(12, "a.bmp"));

    swapchain.TakeFrameDumpOutcomes();
    EXPECT_FALSE(swapchain.IsFrameDumpQueued(12, "a.bmp"));
}

TEST(FrameDumpQueue, ARequestRidesOnTheNextPresentedFrame) {
    FakeSwapchain swapchain;
    swapchain.RequestFrameDump("late.bmp", 10);

    // frame 10 had no drawable, so the next present is 11
    auto job = swapchain.TakeFrameDump(11);
    ASSERT_TRUE(job.has_value());
    job->completion(true);

    const auto outcomes = swapchain.TakeFrameDumpOutcomes();
    ASSERT_EQ(outcomes.size(), 1u);
    EXPECT_EQ(outcomes[0].requested, 10u);
    EXPECT_EQ(outcomes[0].presented, 11u);
    EXPECT_EQ(outcomes[0].path, "late.bmp");
    EXPECT_TRUE(outcomes[0].written);
}

TEST(FrameDumpQueue, OnePresentTakesEveryDueRequest) {
    FakeSwapchain swapchain;
    swapchain.RequestFrameDump("a.bmp", 5);
    swapchain.RequestFrameDump("b.bmp", 6);
    swapchain.RequestFrameDump("c.bmp", 9);

    const auto first = swapchain.TakeFrameDump(7);
    const auto second = swapchain.TakeFrameDump(7);
    ASSERT_TRUE(first.has_value() && second.has_value());
    EXPECT_EQ(first->path, "a.bmp");
    EXPECT_EQ(second->path, "b.bmp");
    EXPECT_FALSE(swapchain.TakeFrameDump(7).has_value());

    EXPECT_EQ(swapchain.PendingFrameDumps(), 3u);

    first->completion(true);
    second->completion(true);
}

TEST(FrameDumpQueue, EachCompletionBecomesOneOutcome) {
    FakeSwapchain swapchain;
    swapchain.RequestFrameDump("ok.bmp", 1);
    swapchain.RequestFrameDump("bad.bmp", 2);
    swapchain.RequestFrameDump("slow.bmp", 3);

    auto ok = swapchain.TakeFrameDump(1);
    auto bad = swapchain.TakeFrameDump(2);
    auto slow = swapchain.TakeFrameDump(3);
    ok->completion(true);
    bad->completion(false);

    const auto outcomes = swapchain.TakeFrameDumpOutcomes();

    ASSERT_EQ(outcomes.size(), 2u);
    EXPECT_EQ(outcomes[0].path, "ok.bmp");
    EXPECT_TRUE(outcomes[0].written);
    EXPECT_EQ(outcomes[1].path, "bad.bmp");
    EXPECT_FALSE(outcomes[1].written);
    EXPECT_TRUE(swapchain.TakeFrameDumpOutcomes().empty());

    slow->completion(true);
    EXPECT_EQ(swapchain.TakeFrameDumpOutcomes().size(), 1u);
}

TEST(FrameDumpQueue, PendingCountsUntilTheOutcomeIsTaken) {
    FakeSwapchain swapchain;
    swapchain.RequestFrameDump("a.bmp", 1);
    swapchain.RequestFrameDump("b.bmp", 2);
    EXPECT_EQ(swapchain.PendingFrameDumps(), 2u);

    auto job = swapchain.TakeFrameDump(1);
    EXPECT_EQ(swapchain.PendingFrameDumps(), 2u);

    job->completion(true);
    EXPECT_EQ(swapchain.PendingFrameDumps(), 2u);

    swapchain.TakeFrameDumpOutcomes();
    EXPECT_EQ(swapchain.PendingFrameDumps(), 1u);
}

TEST(FrameDumpQueue, DestructionWaitsForADumpStillBeingWritten) {
    auto swapchain = std::make_unique<FakeSwapchain>();
    swapchain->RequestFrameDump("slow.bmp", 1);
    auto job = swapchain->TakeFrameDump(1);
    ASSERT_TRUE(job.has_value());

    // the handler thread Metal would run it on
    std::atomic<bool> written = false;
    std::thread handler([&written, completion = std::move(job->completion)] {
        std::this_thread::sleep_for(100ms);
        written = true;
        completion(true);
    });

    swapchain.reset();

    EXPECT_TRUE(written);
    handler.join();
}
