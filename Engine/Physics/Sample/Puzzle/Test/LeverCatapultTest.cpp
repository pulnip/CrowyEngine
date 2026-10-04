#include <gtest/gtest.h>

#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 Cube = 2;
    constexpr u32 Weight = 3;
    constexpr u32 Shelf = 4;
    // the hinge's stop, with the long arm 20 degrees up
    constexpr f32 Stop = 0.6981317f;
    // where the cube's corner clears the ledge's top
    constexpr f32 LedgeTop = 1.8f;

    struct Throw {
        // the weight's fall, and the cube's flight
        f32 weightSpeed = 0.0f;
        f32 cubeSpeed = 0.0f;
        f32 apex = 0.0f;
        f32 turn = 0.0f;
        Vec3 rest = zeros();
        bool solved = false;
    };

    Throw runThrow(PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto cube = session.HandleOf(Cube);
        const auto weight = session.HandleOf(Weight);
        auto run = Throw{};
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            run.weightSpeed = std::max(
                run.weightSpeed,
                norm(world.LinearVelocityOf(weight))
            );
            run.cubeSpeed =
                std::max(run.cubeSpeed, norm(world.LinearVelocityOf(cube)));
            run.apex = std::max(run.apex, world.PoseOf(cube).position.y);
            run.turn = std::max(
                run.turn,
                world.HingeAngleOf(session.HingeHandleOf(0))
            );
        }
        run.rest = world.PoseOf(cube).position;
        run.solved = session.GetStatus().solved;

        return run;
    }

    // the solution with the shelf lifted `lift` instead
    Throw throwFrom(PhysicsRuntime& runtime, f32 lift) {
        constexpr f32 AwayDrop = 0.11276269f;

        auto puzzle = makePuzzle(PuzzleKind::LeverCatapult);
        const auto rest = puzzle.bodies[Shelf].desc.pose.position;
        auto& keys = puzzle.solution.tracks[0].keys;
        keys[0].pose.position.y = rest.y + lift;
        keys[1].pose.position.y = rest.y + lift;
        keys[2].pose.position.y = rest.y + lift - AwayDrop;
        keys[3].pose.position.y = rest.y + lift - AwayDrop;
        PuzzleSession session(runtime, puzzle, PuzzleMode::Solution);

        return runThrow(session);
    }
}

TEST(LeverCatapult, SolutionThrowsOntoTheLedge) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::LeverCatapult,
        PuzzleMode::Solution
    );
    const auto run = runThrow(session);

    // 1.48 m of free fall, about sqrt(2 g h)
    EXPECT_NEAR(run.weightSpeed, 5.4f, 0.2f);
    EXPECT_GT(run.turn, Stop - 0.01f);
    EXPECT_GT(run.cubeSpeed, 3.7f);
    // its lowest corner, 0.106 below its centre, well over the ledge
    EXPECT_GT(run.apex, 2.0f);

    EXPECT_TRUE(run.solved);
    EXPECT_GT(session.GetStatus().solvedAt, 150u);
    EXPECT_LT(session.GetStatus().solvedAt, 240u);
    // at rest on the ledge
    EXPECT_NEAR(run.rest.y, 1.875f, 0.02f);
    EXPECT_GT(run.rest.x, -0.7f + 0.075f);
    EXPECT_LT(run.rest.x, 0.35f - 0.075f);
    EXPECT_EQ(session.GetStatus().tick, PuzzleHorizon);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::LeverCatapult)]
    );
}

TEST(LeverCatapult, ControlTossesTheCubeBack) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::LeverCatapult,
        PuzzleMode::Control
    );
    const auto run = runThrow(session);

    // a 0.28 m drop still turns the lever to its stop, slowly
    EXPECT_LT(run.weightSpeed, 2.6f);
    EXPECT_GT(run.turn, Stop - 0.01f);
    EXPECT_LT(run.apex, LedgeTop - 0.1f);

    EXPECT_FALSE(run.solved);
    // back on the raised long arm
    EXPECT_LT(run.rest.y, 1.5f);
    EXPECT_LT(run.rest.x, -0.15f);
    EXPECT_EQ(session.GetStatus().tick, PuzzleHorizon);
}

TEST(LeverCatapult, HigherDropsThrowHigher) {
    PhysicsRuntime runtime;
    f32 lastApex = 0.0f;
    u32 runs = 0;
    for(const f32 lift: {0.0f, 0.6f, 1.0f, 1.2f, 1.4f, 1.6f}) {
        const auto run = throwFrom(runtime, lift);
        EXPECT_GT(run.apex, lastApex) << "lift " << lift;
        // below about 0.8 m the cube falls short of the ledge
        EXPECT_EQ(run.solved, lift > 0.8f) << "lift " << lift;
        lastApex = run.apex;
        ++runs;
    }
    EXPECT_EQ(runs, 6u);
}
