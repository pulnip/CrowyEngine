#include <cmath>
#include <optional>

#include <gtest/gtest.h>

#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 Buoy = 6;
    constexpr u32 Plunger = 7;
    constexpr f32 BuoyRadius = 0.2f;
    constexpr f32 Surface = 1.2f;
    // where the plunger lets go
    constexpr u64 HoldEnds = 90;

    // what one run of the buoy shows after the hold
    struct BuoyRun {
        f32 heldAt = 0.0f;
        // rising, as its bottom comes out of the water
        std::optional<f32> surfacingSpeed;
        f32 apex = 0.0f;
        // off the plunger's line, which a clean rise never leaves
        f32 drift = 0.0f;
        // whether the buoy's top went under while held
        bool submerged = false;
    };

    BuoyRun runBuoy(PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto buoy = session.HandleOf(Buoy);
        auto run = BuoyRun{};
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            const auto tick = session.GetStatus().tick;
            const auto position = world.PoseOf(buoy).position;
            const auto velocity = world.LinearVelocityOf(buoy);
            run.drift = std::max(run.drift, std::abs(position.x + 0.3f));
            if(tick <= HoldEnds) {
                run.submerged |= position.y + BuoyRadius < Surface;
                if(tick == HoldEnds)
                    run.heldAt = position.y;
                continue;
            }
            if(!run.surfacingSpeed && velocity.y > 0.0f &&
               position.y - BuoyRadius >= Surface)
                run.surfacingSpeed = velocity.y;
            // the first rise's top
            if(!run.surfacingSpeed || velocity.y > 0.0f)
                run.apex = std::max(run.apex, position.y);
        }

        return run;
    }
}

TEST(BuoyPop, FloatsWhereItStarts) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::BuoyPop, PuzzleMode::Solution);
    const auto& world = session.GetWorld();
    const auto buoy = session.HandleOf(Buoy);
    // the plunger, a centimeter above, has not reached it yet
    for(u64 tick = 1; tick <= 4; ++tick) {
        session.Tick();
        EXPECT_NEAR(world.PoseOf(buoy).position.y, 1.255154f, 0.005f)
            << "at tick " << tick;
        EXPECT_LT(std::abs(world.LinearVelocityOf(buoy).y), 0.05f);
    }
    EXPECT_EQ(session.GetStatus().tick, 4u);
}

TEST(BuoyPop, SolutionPopsThroughTheHoop) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::BuoyPop, PuzzleMode::Solution);
    const auto run = runBuoy(session);

    // 0.6 m deep, wholly under
    EXPECT_NEAR(run.heldAt, 0.6f, 0.01f);
    EXPECT_TRUE(run.submerged);
    ASSERT_TRUE(run.surfacingSpeed.has_value());
    EXPECT_GT(*run.surfacingSpeed, 3.5f);
    EXPECT_LT(*run.surfacingSpeed, 4.3f);
    // its top clears the hoop's 1.7 by more than its own height
    EXPECT_GT(run.apex, 2.0f);
    EXPECT_LT(run.drift, 0.01f);

    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GT(status.solvedAt, HoldEnds);
    EXPECT_LT(status.solvedAt, HoldEnds + 45);
    const auto plunger = session.GetWorld().PoseOf(session.HandleOf(Plunger));
    EXPECT_NEAR(plunger.position.x, 0.4f, 1.0e-4f);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::BuoyPop)]
    );
}

TEST(BuoyPop, ControlStopsShortOfTheHoop) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::BuoyPop, PuzzleMode::Control);
    const auto run = runBuoy(session);

    // 0.15 m deep: its top stays out of the water
    EXPECT_NEAR(run.heldAt, 1.05f, 0.01f);
    EXPECT_FALSE(run.submerged);
    ASSERT_TRUE(run.surfacingSpeed.has_value());
    EXPECT_LT(*run.surfacingSpeed, 1.6f);
    // its top only reaches the hoop
    EXPECT_LT(run.apex, 1.6f);
    EXPECT_LT(run.drift, 0.01f);

    EXPECT_FALSE(session.GetStatus().solved);
    EXPECT_EQ(session.GetStatus().tick, PuzzleHorizon);
}
