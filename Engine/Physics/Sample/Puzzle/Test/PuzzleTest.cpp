#include <algorithm>
#include <cmath>
#include <format>
#include <print>
#include <variant>

#include <gtest/gtest.h>

#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr std::array<usize, 3> BodyCounts{6, 7, 6};
    constexpr std::array<usize, 3> HingeCounts{0, 0, 1};

    usize indexOf(PuzzleKind kind) {
        return static_cast<usize>(kind);
    }

    bool isInside(const AABB3D& bounds, f32 lowestY) {
        constexpr f32 Slack = 1.0e-4f;
        const auto low = bounds.center - bounds.halfScale;
        const auto high = bounds.center + bounds.halfScale;

        return low.x >= -PuzzleHalfWidth - Slack &&
               high.x <= PuzzleHalfWidth + Slack &&
               low.z >= -PuzzleHalfWidth - Slack &&
               high.z <= PuzzleHalfWidth + Slack && low.y >= lowestY &&
               high.y <= PuzzleHeight;
    }

    BodyPose poseOfBody(const PuzzleSession& session, u32 body) {
        return session.GetWorld().PoseOf(session.HandleOf(body));
    }
}

TEST(PuzzleBuild, SessionMirrorsItsPuzzle) {
    PhysicsRuntime runtime;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            SCOPED_TRACE(std::format("{} {}", enumName(kind), enumName(mode)));
            const PuzzleSession session(runtime, kind, mode);
            const auto& puzzle = session.GetPuzzle();
            const auto& world = session.GetWorld();

            EXPECT_EQ(puzzle.bodies.size(), BodyCounts[indexOf(kind)]);
            EXPECT_EQ(world.BodyCount(), puzzle.bodies.size());
            EXPECT_EQ(puzzle.hinges.size(), HingeCounts[indexOf(kind)]);
            for(u32 i = 0; i < puzzle.bodies.size(); ++i) {
                const auto& desc = puzzle.bodies[i].desc;
                const auto body = session.HandleOf(i);
                ASSERT_TRUE(world.IsValid(body));
                EXPECT_EQ(world.MotionOf(body), desc.motion);
                EXPECT_EQ(world.ShapeOf(body).index(), desc.shape.index());
                EXPECT_EQ(world.PoseOf(body).position, desc.pose.position);
                EXPECT_EQ(world.PoseOf(body).rotation, desc.pose.rotation);
            }
            for(u32 i = 0; i < puzzle.hinges.size(); ++i)
                EXPECT_TRUE(world.IsValid(session.HingeHandleOf(i)));

            EXPECT_EQ(session.GetStatus().tick, 0u);
            EXPECT_EQ(world.TickCount(), 0u);
            EXPECT_EQ(session.GetStatus().hash, world.StateHash());
            EXPECT_FALSE(session.GetStatus().solved);
        }
    }
}

TEST(PuzzleBuild, ModesShareTheBuild) {
    PhysicsRuntime runtime;
    for(const auto kind: AllPuzzleKinds) {
        SCOPED_TRACE(enumName(kind));
        const PuzzleSession solution(runtime, kind, PuzzleMode::Solution);
        const PuzzleSession control(runtime, kind, PuzzleMode::Control);
        EXPECT_EQ(solution.GetStatus().hash, control.GetStatus().hash);
        EXPECT_EQ(
            solution.GetWorld().BodyCount(),
            control.GetWorld().BodyCount()
        );

        const auto& puzzle = solution.GetPuzzle();
        ASSERT_EQ(puzzle.solution.size(), 2u);
        ASSERT_EQ(puzzle.control.size(), 1u);
        // the control is the solution without the mechanism's move
        EXPECT_EQ(puzzle.control[0].tick, puzzle.solution[1].tick);
        EXPECT_EQ(puzzle.control[0].body, puzzle.solution[1].body);
        EXPECT_EQ(puzzle.control[0].impulse, puzzle.solution[1].impulse);
    }
}

TEST(PuzzleBuild, InsideFootprint) {
    for(const auto kind: AllPuzzleKinds) {
        SCOPED_TRACE(enumName(kind));
        const auto puzzle = makePuzzle(kind);
        for(const auto& body: puzzle.bodies) {
            SCOPED_TRACE(body.name);
            const auto halfExtent =
                std::holds_alternative<BoxShape>(body.desc.shape)
                    ? std::get<BoxShape>(body.desc.shape).halfExtent
                    : ones() *
                          std::get<SphereShape>(body.desc.shape).radius;
            // a floor reaches below the origin; everything else stands on it
            const auto lowestY =
                body.role == PieceRole::Ground ? -0.5f : -1.0e-4f;
            const auto bounds = AABB3D{
                .center = body.desc.pose.position,
                .halfScale = halfExtent,
            };
            EXPECT_TRUE(isInside(bounds, lowestY));
        }
        for(const auto& zone: puzzle.zones) {
            SCOPED_TRACE(zone.name);
            EXPECT_TRUE(isInside(
                AABB3D{.center = zone.center, .halfScale = zone.halfExtent},
                0.0f
            ));
        }
        for(const auto mode: AllPuzzleModes) {
            for(const auto& input: inputsOf(puzzle, mode)) {
                EXPECT_LT(input.tick, PuzzleHorizon);
                EXPECT_EQ(
                    puzzle.bodies[input.body].desc.motion,
                    BodyMotion::Dynamic
                );
            }
        }
    }
}

TEST(PuzzleRun, PlateGateSolutionOpensTheGate) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::PlateGate, PuzzleMode::Solution);
    const auto& puzzle = session.GetPuzzle();
    const auto& plate = puzzle.zones[puzzle.plates[0].zone];

    runTo(session, PuzzleHorizon);
    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GT(status.solvedAt, puzzle.solution[1].tick);
    EXPECT_LE(status.solvedAt, PuzzleHorizon);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(session.GetWorld().BodyCount(), 6u);

    const auto crate = session.HandleOf(4);
    EXPECT_TRUE(std::ranges::contains(
        session.GetWorld().Overlapping(plate.center, plate.halfExtent),
        crate
    ));
    EXPECT_EQ(session.PlateLevelOf(0), 50u);
    EXPECT_NEAR(poseOfBody(session, 3).position.y, 0.81f, 1.0e-5f);
}

TEST(PuzzleRun, PlateGateControlStaysShut) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::PlateGate, PuzzleMode::Control);
    const auto& puzzle = session.GetPuzzle();
    const auto ballStart = puzzle.bodies[5].desc.pose.position;
    const auto crateStart = puzzle.bodies[4].desc.pose.position;

    while(session.GetStatus().tick < PuzzleHorizon) {
        session.Tick();
        ASSERT_EQ(session.PlateLevelOf(0), 0u);
    }
    const auto& status = session.GetStatus();
    EXPECT_FALSE(status.solved);
    EXPECT_EQ(status.solvedAt, 0u);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(session.GetWorld().BodyCount(), 6u);

    const auto gateStart = puzzle.bodies[3].desc.pose.position;
    EXPECT_EQ(poseOfBody(session, 3).position, gateStart);
    EXPECT_LT(distance(poseOfBody(session, 4).position, crateStart), 1.0e-3f);
    // the ball took its push and rolled up to the gate
    EXPECT_GT(distance(poseOfBody(session, 5).position, ballStart), 0.05f);
}

TEST(PuzzleRun, ToppleBridgeSolutionLaysTheBridge) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::ToppleBridge,
        PuzzleMode::Solution
    );
    const auto& puzzle = session.GetPuzzle();

    runTo(session, PuzzleHorizon);
    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GT(status.solvedAt, puzzle.solution[1].tick);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(session.GetWorld().BodyCount(), 7u);

    // lying across the gap: centered over it, a quarter turn about z
    const auto slab = poseOfBody(session, 5);
    EXPECT_LE(std::abs(slab.position.x), 0.15f);
    EXPECT_GE(slab.position.y, 0.55f);
    EXPECT_LE(slab.position.y, 0.65f);
    EXPECT_GE(std::abs(slab.rotation.z), 0.68f);
    EXPECT_LE(std::abs(slab.rotation.z), 0.73f);
}

TEST(PuzzleRun, ToppleBridgeControlFallsIntoTheGap) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::ToppleBridge,
        PuzzleMode::Control
    );
    const auto ballStart = session.GetPuzzle().bodies[6].desc.pose.position;

    while(session.GetStatus().tick < PuzzleHorizon) {
        session.Tick();
        ASSERT_LT(std::abs(poseOfBody(session, 5).rotation.z), 0.01f);
    }
    const auto& status = session.GetStatus();
    EXPECT_FALSE(status.solved);
    EXPECT_EQ(status.solvedAt, 0u);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(session.GetWorld().BodyCount(), 7u);

    const auto ball = poseOfBody(session, 6).position;
    EXPECT_GT(distance(ball, ballStart), 0.05f);
    EXPECT_LT(std::abs(ball.x), 0.4f);
    EXPECT_LT(ball.y, 0.5f);
}

TEST(PuzzleRun, SwingDoorSolutionOpensTheDoor) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::SwingDoor, PuzzleMode::Solution);
    const auto& puzzle = session.GetPuzzle();

    runTo(session, PuzzleHorizon);
    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    EXPECT_GT(status.solvedAt, puzzle.solution[1].tick);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(session.GetWorld().BodyCount(), 6u);
    EXPECT_GE(session.GetWorld().HingeAngleOf(session.HingeHandleOf(0)), 1.4f);
}

TEST(PuzzleRun, SwingDoorControlDoorHolds) {
    PhysicsRuntime runtime;
    PuzzleSession session(runtime, PuzzleKind::SwingDoor, PuzzleMode::Control);
    const auto ballStart = session.GetPuzzle().bodies[5].desc.pose.position;

    while(session.GetStatus().tick < PuzzleHorizon) {
        session.Tick();
        ASSERT_LT(
            session.GetWorld().HingeAngleOf(session.HingeHandleOf(0)),
            0.1f
        );
    }
    const auto& status = session.GetStatus();
    EXPECT_FALSE(status.solved);
    EXPECT_EQ(status.solvedAt, 0u);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(session.GetWorld().BodyCount(), 6u);

    const auto ball = poseOfBody(session, 5).position;
    EXPECT_GT(distance(ball, ballStart), 0.05f);
    EXPECT_LT(ball.x, 0.0f);
}

TEST(PuzzleRun, StaysInFootprint) {
    PhysicsRuntime runtime;
    u64 ticks = 0;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            SCOPED_TRACE(std::format("{} {}", enumName(kind), enumName(mode)));
            PuzzleSession session(runtime, kind, mode);
            const auto& puzzle = session.GetPuzzle();
            while(session.GetStatus().tick < PuzzleHorizon) {
                session.Tick();
                ++ticks;
                for(u32 i = 0; i < puzzle.bodies.size(); ++i) {
                    if(puzzle.bodies[i].desc.motion == BodyMotion::Static)
                        continue;
                    // resting contacts sink up to the 0.02 m slop
                    ASSERT_TRUE(isInside(
                        worldBoundsOf(session.GetWorld(), session.HandleOf(i)),
                        -0.05f
                    )) << puzzle.bodies[i].name << " at tick "
                       << session.GetStatus().tick;
                }
            }
        }
    }
    EXPECT_EQ(ticks, 6u * PuzzleHorizon);
}

TEST(PuzzleDeterminism, RunsRepeatPerTick) {
    PhysicsRuntime runtime;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            SCOPED_TRACE(std::format("{} {}", enumName(kind), enumName(mode)));
            PuzzleSession first(runtime, kind, mode);
            PuzzleSession second(runtime, kind, mode);
            u64 compared = 0;
            while(first.GetStatus().tick < PuzzleHorizon) {
                first.Tick();
                second.Tick();
                ASSERT_EQ(first.GetStatus().hash, second.GetStatus().hash)
                    << "at tick " << first.GetStatus().tick;
                ++compared;
            }
            EXPECT_EQ(compared, PuzzleHorizon);
            EXPECT_EQ(first.GetStatus(), second.GetStatus());
        }
    }
}

TEST(PuzzleDeterminism, WorkerCountsAgree) {
    auto runAll = [](u32 workers) {
        PhysicsRuntime runtime(PhysicsRuntimeDesc{.workerThreads = workers});
        std::vector<TickHashes> runs;
        for(const auto kind: AllPuzzleKinds) {
            for(const auto mode: AllPuzzleModes) {
                PuzzleSession session(runtime, kind, mode);
                runs.push_back(hashesToHorizon(session));
            }
        }
        return runs;
    };

    const auto alone = runAll(0);
    const auto shared = runAll(3);
    ASSERT_EQ(alone.size(), 6u);
    for(const auto& run: alone)
        EXPECT_EQ(run.size(), PuzzleHorizon + 1);
    EXPECT_EQ(alone, shared);
}

// prints where the moving bodies are every 30 ticks, for tuning by hand
TEST(PuzzleTrace, DISABLED_Print) {
    PhysicsRuntime runtime;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            std::println("== {} {}", enumName(kind), enumName(mode));
            PuzzleSession session(runtime, kind, mode);
            const auto& puzzle = session.GetPuzzle();
            while(session.GetStatus().tick < PuzzleHorizon) {
                session.Tick();
                if(session.GetStatus().tick % 30 != 0)
                    continue;
                std::print(
                    "t{:3} solved {} ",
                    session.GetStatus().tick,
                    session.GetStatus().solvedAt
                );
                for(u32 i = 0; i < puzzle.bodies.size(); ++i) {
                    if(puzzle.bodies[i].desc.motion == BodyMotion::Static)
                        continue;
                    const auto pose = poseOfBody(session, i);
                    std::print(
                        "| {} ({:.3f} {:.3f} {:.3f}) q({:.3f} {:.3f} {:.3f} "
                        "{:.3f}) ",
                        puzzle.bodies[i].name,
                        pose.position.x,
                        pose.position.y,
                        pose.position.z,
                        pose.rotation.x,
                        pose.rotation.y,
                        pose.rotation.z,
                        pose.rotation.w
                    );
                }
                if(!puzzle.hinges.empty()) {
                    const auto hinge = session.HingeHandleOf(0);
                    std::print(
                        "| hinge {:.3f}",
                        session.GetWorld().HingeAngleOf(hinge)
                    );
                }
                std::println("");
            }
        }
    }
}
