#include <optional>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"
#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 Tray = 1;
    constexpr u32 Marble = 7;
    constexpr f32 MarbleRadius = 0.1f;

    // the marble where the tray would see it: its floor's top is y 0
    Vec3 inTray(const PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto tray = world.PoseOf(session.HandleOf(Tray));
        const auto marble = world.PoseOf(session.HandleOf(Marble)).position;
        const auto toTray =
            inverseRigid(modelMat(tray.position, tray.rotation, ones()));

        return static_cast<Vec3>(toTray * toVec4(marble, 1.0f));
    }

    // the first tick at which `reached` holds, if any
    template<typename Reached>
    std::optional<u64> firstTick(PuzzleSession& session, Reached reached) {
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            if(reached(session))
                return session.GetStatus().tick;
        }

        return std::nullopt;
    }
}

TEST(TiltTray, SolutionRollsRoundTheDivider) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::TiltTray, PuzzleMode::Solution);

    // far side down: it rolls to the far rim, past the divider's end
    const auto atFarRim = firstTick(session, [](const PuzzleSession& run) {
        return inTray(run).z >= 0.6f;
    });
    ASSERT_TRUE(atFarRim.has_value());
    EXPECT_LE(*atFarRim, 135u);
    // right side down too: along the far rim into the right lane
    const auto inRightLane = firstTick(session, [](const PuzzleSession& run) {
        return inTray(run).x >= 0.6f;
    });
    ASSERT_TRUE(inRightLane.has_value());
    EXPECT_GT(*inRightLane, 170u);
    EXPECT_LE(*inRightLane, 275u);
    // near side down: down the right lane and out through the gap
    const auto offTheTray = firstTick(session, [](const PuzzleSession& run) {
        const auto& world = run.GetWorld();
        return world.PoseOf(run.HandleOf(Marble)).position.y < 0.8f;
    });
    ASSERT_TRUE(offTheTray.has_value());
    EXPECT_GT(*offTheTray, 320u);

    runTo(session, PuzzleHorizon);
    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GE(status.solvedAt, *offTheTray);
    EXPECT_LT(status.solvedAt, *offTheTray + 30);
    // at rest in the cup
    const auto rest =
        session.GetWorld().PoseOf(session.HandleOf(Marble)).position;
    EXPECT_NEAR(rest.y, 0.3f + MarbleRadius, 0.02f);
    EXPECT_GT(rest.x, 0.4f);
    EXPECT_LT(rest.x, 1.0f);
    EXPECT_GT(rest.z, -1.4f);
    EXPECT_LT(rest.z, -0.8f);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::TiltTray)]
    );
}

TEST(TiltTray, ControlStaysLeftOfTheDivider) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::TiltTray, PuzzleMode::Control);
    const auto& world = session.GetWorld();

    // never tilted far side down, so it never reaches the passage
    u64 checked = 0;
    while(session.GetStatus().tick < PuzzleHorizon) {
        session.Tick();
        const auto local = inTray(session);
        EXPECT_LE(local.z, -0.6f) << "at tick " << session.GetStatus().tick;
        if(session.GetStatus().tick >= 240)
            EXPECT_LE(local.x, -0.1f) << "at tick " << session.GetStatus().tick;
        EXPECT_GE(world.PoseOf(session.HandleOf(Marble)).position.y, 0.9f);
        ++checked;
    }
    EXPECT_EQ(checked, PuzzleHorizon);
    EXPECT_FALSE(session.GetStatus().solved);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::TiltTray)]
    );
}
