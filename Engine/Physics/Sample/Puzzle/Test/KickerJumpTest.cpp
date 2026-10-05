#include <optional>

#include <gtest/gtest.h>

#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 Marble = 8;
    constexpr f32 MarbleRadius = 0.1f;
    // the front wall's near face; the kicker ends 3 cm short of it
    constexpr f32 WallX = 0.65f;

    struct MarbleRun {
        // halfway along the level runway, past the ramp's kink
        std::optional<f32> runwaySpeed;
        f32 apex = 0.0f;
        // where it crosses the wall's line, if it does
        std::optional<f32> heightAtWall;
        Vec3 rest = zeros();
    };

    MarbleRun runMarble(PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto marble = session.HandleOf(Marble);
        auto run = MarbleRun{};
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            const auto position = world.PoseOf(marble).position;
            if(!run.runwaySpeed && position.x >= 0.02f)
                run.runwaySpeed = norm(world.LinearVelocityOf(marble));
            // past the runway, where the kicker lifts it or not
            if(position.x > 0.3f)
                run.apex = std::max(run.apex, position.y);
            if(!run.heightAtWall && position.x >= WallX + 0.02f)
                run.heightAtWall = position.y;
        }
        run.rest = world.PoseOf(marble).position;

        return run;
    }
}

TEST(KickerJump, SolutionJumpsTheWallIntoTheCup) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::KickerJump,
        PuzzleMode::Solution
    );
    const auto run = runMarble(session);

    // 1.05 m of drop at 5/7 g, less what the 40 degree kink takes
    ASSERT_TRUE(run.runwaySpeed.has_value());
    EXPECT_GT(*run.runwaySpeed, 2.8f);
    EXPECT_LT(*run.runwaySpeed, 3.4f);
    // up the kicker and over the wall's 0.77 top with room
    EXPECT_GT(run.apex, 0.93f);
    ASSERT_TRUE(run.heightAtWall.has_value());
    EXPECT_GT(*run.heightAtWall, 0.87f);

    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GT(status.solvedAt, 120u);
    EXPECT_LT(status.solvedAt, 180u);
    // on the cup's floor, between the walls
    EXPECT_NEAR(run.rest.y, 0.5f + MarbleRadius, 0.02f);
    EXPECT_GT(run.rest.x, 0.69f + MarbleRadius);
    EXPECT_LT(run.rest.x, 1.38f - MarbleRadius + 0.02f);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::KickerJump)]
    );
}

TEST(KickerJump, ControlStopsAtTheWallsFace) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::KickerJump, PuzzleMode::Control);
    const auto run = runMarble(session);

    // the same drop, the same speed
    ASSERT_TRUE(run.runwaySpeed.has_value());
    EXPECT_GT(*run.runwaySpeed, 2.8f);
    EXPECT_LT(*run.runwaySpeed, 3.4f);
    // a level kicker cannot lift it: it meets the frictionless face
    EXPECT_LT(run.apex, 0.71f);
    EXPECT_FALSE(run.heightAtWall.has_value());

    EXPECT_FALSE(session.GetStatus().solved);
    // on the kicker, against the wall; the hit may sink 2 cm
    EXPECT_NEAR(run.rest.y, 0.6f + MarbleRadius, 0.02f);
    EXPECT_GT(run.rest.x, WallX - MarbleRadius - 0.01f);
    EXPECT_LT(run.rest.x, WallX - MarbleRadius + 0.025f);
    EXPECT_EQ(session.GetStatus().tick, PuzzleHorizon);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::KickerJump)]
    );
}
