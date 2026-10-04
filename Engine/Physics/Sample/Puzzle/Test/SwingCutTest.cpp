#include <cmath>
#include <numbers>
#include <optional>

#include <gtest/gtest.h>

#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 Bob = 8;
    constexpr Vec3 Pivot{-0.25f, 3.2f, 0.0f};
    constexpr f32 RodLength = 1.4f;
    constexpr f32 BobRadius = 0.15f;
    // the bob's centre clears the front wall's top edge above this
    constexpr f32 OverTheWall = 1.45f + BobRadius;

    // a frictionless pendulum's speed at `angle` past its start, 50 degrees
    // out; a ball's own inertia adds 2/5 (r/L)^2
    f32 pendulumSpeed(f32 angle) {
        const auto start = std::numbers::pi_v<f32> * 50.0f / 180.0f;
        const auto inertia =
            1.0f + 0.4f * (BobRadius / RodLength) * (BobRadius / RodLength);
        const auto drop =
            RodLength * (std::cos(angle - start) - std::cos(start));

        return std::sqrt(2.0f * 9.81f * drop / inertia);
    }

    struct CutState {
        f32 angle = 0.0f;
        Vec3 velocity = zeros();
    };

    // runs to the cut and reads the swing just before it
    CutState runToCut(PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto& release =
            scriptOf(session.GetPuzzle(), session.GetMode()).releases[0];
        runTo(session, release.tick);
        EXPECT_TRUE(world.IsHingeHeld(session.HingeHandleOf(0)));

        return CutState{
            .angle = world.HingeAngleOf(session.HingeHandleOf(0)),
            .velocity = world.LinearVelocityOf(session.HandleOf(Bob)),
        };
    }

    // the bob's centre height when it first reaches `x`, if it does
    std::optional<f32> heightAt(PuzzleSession& session, f32 x) {
        const auto& world = session.GetWorld();
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            const auto position = world.PoseOf(session.HandleOf(Bob)).position;
            if(position.x >= x)
                return position.y;
        }

        return std::nullopt;
    }

    bool solvesWithCutAt(PhysicsRuntime& runtime, u64 tick) {
        auto puzzle = makePuzzle(PuzzleKind::SwingCut);
        puzzle.solution.releases[0].tick = tick;
        PuzzleSession session(runtime, puzzle, PuzzleMode::Solution);
        runTo(session, PuzzleHorizon);

        return session.GetStatus().solved;
    }
}

TEST(SwingCut, SolutionThrowsTheBobOverTheWall) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::SwingCut, PuzzleMode::Solution);
    const auto& world = session.GetWorld();
    const auto bob = session.HandleOf(Bob);

    // a quarter of the way up the far side, moving up and right
    const auto cut = runToCut(session);
    EXPECT_NEAR(cut.angle, 1.32f, 0.02f);
    EXPECT_NEAR(norm(cut.velocity), pendulumSpeed(cut.angle), 0.05f);
    EXPECT_GT(cut.velocity.x, 0.0f);
    EXPECT_GT(cut.velocity.y, 0.3f * cut.velocity.x);

    // free of its circle within ten ticks
    const auto cutTick = session.GetStatus().tick;
    runTo(session, cutTick + 10);
    EXPECT_FALSE(world.IsHingeHeld(session.HingeHandleOf(0)));
    EXPECT_GT(distance(world.PoseOf(bob).position, Pivot), RodLength + 0.05f);

    const auto overTheWall = heightAt(session, 0.925f);
    ASSERT_TRUE(overTheWall.has_value());
    EXPECT_GT(*overTheWall, OverTheWall);

    runTo(session, PuzzleHorizon);
    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GT(status.solvedAt, cutTick);
    EXPECT_LT(status.solvedAt, cutTick + 60);
    // at rest on the pad, between the walls
    const auto rest = world.PoseOf(bob).position;
    EXPECT_NEAR(rest.y, 1.15f + BobRadius, 0.02f);
    EXPECT_GE(rest.x, 0.95f + BobRadius - 0.02f);
    EXPECT_LE(rest.x, 1.4f - BobRadius + 0.02f);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(
        world.BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::SwingCut)]
    );
}

TEST(SwingCut, ControlLeavesLevelIntoTheWall) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::SwingCut, PuzzleMode::Control);
    const auto& world = session.GetWorld();
    const auto bob = session.HandleOf(Bob);

    // the bottom of the swing: fastest, and level
    const auto cut = runToCut(session);
    EXPECT_NEAR(cut.angle, std::numbers::pi_v<f32> * 50.0f / 180.0f, 0.02f);
    EXPECT_NEAR(norm(cut.velocity), pendulumSpeed(cut.angle), 0.05f);
    EXPECT_LT(std::abs(cut.velocity.y), 0.05f * cut.velocity.x);

    // it meets the wall's face below the top edge and never crosses
    const auto atTheWall = heightAt(session, 0.9f - BobRadius - 0.02f);
    ASSERT_TRUE(atTheWall.has_value());
    EXPECT_LT(*atTheWall, OverTheWall - 0.1f);
    f32 farthest = -PuzzleHalfWidth;
    while(session.GetStatus().tick < PuzzleHorizon) {
        session.Tick();
        farthest = std::max(farthest, world.PoseOf(bob).position.x);
    }
    // a hit at 5 cm a tick sinks up to 4 cm into the face
    EXPECT_LT(farthest, 0.9f - BobRadius + 0.04f);

    EXPECT_FALSE(session.GetStatus().solved);
    // rolled back to the curb on the floor
    const auto rest = world.PoseOf(bob).position;
    EXPECT_NEAR(rest.x, -1.4f + BobRadius, 0.02f);
    EXPECT_NEAR(rest.y, BobRadius, 0.025f);
    EXPECT_EQ(session.GetStatus().tick, PuzzleHorizon);
}

TEST(SwingCut, ScoresAnywhereBetweenTheWallAndTheTurn) {
    PhysicsRuntime runtime;
    u32 runs = 0;
    // earlier the throw is too flat to clear the wall
    for(const u64 tick: {38u, 40u}) {
        EXPECT_FALSE(solvesWithCutAt(runtime, tick)) << "cut at " << tick;
        ++runs;
    }
    for(const u64 tick: {46u, 50u, 54u, 58u, 62u, 66u}) {
        EXPECT_TRUE(solvesWithCutAt(runtime, tick)) << "cut at " << tick;
        ++runs;
    }
    // at the turn the bob drops short of the wall
    for(const u64 tick: {74u, 78u}) {
        EXPECT_FALSE(solvesWithCutAt(runtime, tick)) << "cut at " << tick;
        ++runs;
    }
    EXPECT_EQ(runs, 10u);
}
