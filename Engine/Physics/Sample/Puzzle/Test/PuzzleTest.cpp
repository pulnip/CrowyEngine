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

            EXPECT_EQ(puzzle.bodies.size(), PuzzleBodyCounts[indexOf(kind)]);
            EXPECT_EQ(world.BodyCount(), puzzle.bodies.size());
            EXPECT_EQ(puzzle.hinges.size(), PuzzleHingeCounts[indexOf(kind)]);
            for(u32 i = 0; i < puzzle.bodies.size(); ++i) {
                const auto& desc = puzzle.bodies[i].desc;
                const auto body = session.HandleOf(i);
                ASSERT_TRUE(world.IsValid(body));
                EXPECT_EQ(world.MotionOf(body), desc.motion);
                EXPECT_EQ(world.ShapeOf(body).index(), desc.shape.index());
                EXPECT_EQ(world.PoseOf(body).position, desc.pose.position);
                EXPECT_EQ(world.PoseOf(body).rotation, desc.pose.rotation);
            }
            for(u32 i = 0; i < puzzle.hinges.size(); ++i) {
                EXPECT_TRUE(world.IsValid(session.HingeHandleOf(i)));
                EXPECT_TRUE(world.IsHingeHeld(session.HingeHandleOf(i)));
            }

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
        const auto bodies = PuzzleBodyCounts[indexOf(kind)];
        EXPECT_EQ(solution.GetWorld().BodyCount(), bodies);
        EXPECT_EQ(control.GetWorld().BodyCount(), bodies);
        EXPECT_EQ(solution.GetStatus().tick, 0u);
        EXPECT_EQ(control.GetStatus().tick, 0u);
    }
}

TEST(PuzzleBuild, InsideFootprint) {
    for(const auto kind: AllPuzzleKinds) {
        SCOPED_TRACE(enumName(kind));
        const auto puzzle = makePuzzle(kind);
        for(const auto& body: puzzle.bodies) {
            SCOPED_TRACE(body.name);
            // a floor reaches below the origin; everything else stands on it
            const auto lowestY =
                body.role == PieceRole::Ground ? -0.5f : -1.0e-4f;
            EXPECT_TRUE(isInside(
                boundsOf(body.desc.shape, body.desc.pose),
                lowestY
            ));
        }
        for(const auto& zone: puzzle.zones) {
            SCOPED_TRACE(zone.name);
            EXPECT_TRUE(isInside(
                AABB3D{.center = zone.center, .halfScale = zone.halfExtent},
                0.0f
            ));
        }
        for(const auto& water: puzzle.waters) {
            EXPECT_TRUE(isInside(
                AABB3D{.center = water.center, .halfScale = water.halfExtent},
                0.0f
            ));
        }
        for(const auto mode: AllPuzzleModes) {
            const auto& script = scriptOf(puzzle, mode);
            for(const auto& track: script.tracks) {
                const auto& body = puzzle.bodies[track.body];
                SCOPED_TRACE(body.name);
                for(const auto& key: track.keys) {
                    EXPECT_LT(key.tick, PuzzleHorizon);
                    EXPECT_TRUE(
                        isInside(boundsOf(body.desc.shape, key.pose), 0.0f)
                    ) << "key at tick " << key.tick;
                }
            }
            for(const auto& release: script.releases) {
                EXPECT_LT(release.tick, PuzzleHorizon);
                EXPECT_LT(release.hinge, puzzle.hinges.size());
            }
        }
    }
}

TEST(PuzzleRun, StaysInFootprint) {
    PhysicsRuntime runtime;
    u64 ticks = 0;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            SCOPED_TRACE(std::format("{} {}", enumName(kind), enumName(mode)));
            PuzzleSession session(runtime, kind, mode);
            const auto& puzzle = session.GetPuzzle();
            u64 checked = 0;
            while(session.GetStatus().tick < PuzzleHorizon) {
                session.Tick();
                ++ticks;
                for(u32 i = 0; i < puzzle.bodies.size(); ++i) {
                    if(puzzle.bodies[i].desc.motion == BodyMotion::Static)
                        continue;
                    ++checked;
                    // resting contacts sink up to the 0.02 m slop
                    ASSERT_TRUE(isInside(
                        worldBoundsOf(session.GetWorld(), session.HandleOf(i)),
                        -0.05f
                    )) << puzzle.bodies[i].name << " at tick "
                       << session.GetStatus().tick;
                }
            }
            const auto moving = PuzzleMovingCounts[indexOf(kind)];
            EXPECT_EQ(checked, moving * PuzzleHorizon);
            EXPECT_EQ(
                session.GetWorld().BodyCount(),
                PuzzleBodyCounts[indexOf(kind)]
            );
        }
    }
    EXPECT_EQ(ticks, AllPuzzleKinds.size() * 2u * PuzzleHorizon);
}

TEST(PuzzleRun, AppliesEveryEventInBothModes) {
    PhysicsRuntime runtime;
    usize events = 0;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            SCOPED_TRACE(std::format("{} {}", enumName(kind), enumName(mode)));
            PuzzleSession session(runtime, kind, mode);
            const auto& script = scriptOf(session.GetPuzzle(), mode);
            runTo(session, PuzzleHorizon);
            EXPECT_EQ(session.GetStatus().eventsApplied, eventCountOf(script));
            for(const auto& release: script.releases) {
                EXPECT_FALSE(session.GetWorld().IsHingeHeld(
                    session.HingeHandleOf(release.hinge)
                ));
            }
            events += session.GetStatus().eventsApplied;
        }
    }
    // SwingCut cuts once in each mode
    EXPECT_EQ(events, 2u);
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
            EXPECT_EQ(
                first.GetWorld().BodyCount(),
                PuzzleBodyCounts[indexOf(kind)]
            );
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
                EXPECT_EQ(
                    session.GetWorld().BodyCount(),
                    PuzzleBodyCounts[indexOf(kind)]
                );
            }
        }
        return runs;
    };

    const auto alone = runAll(0);
    const auto shared = runAll(3);
    ASSERT_EQ(alone.size(), AllPuzzleKinds.size() * AllPuzzleModes.size());
    for(const auto& run: alone)
        EXPECT_EQ(run.size(), PuzzleHorizon + 1);
    EXPECT_EQ(alone, shared);
}

// prints where the moving bodies are every 10 ticks, for tuning by hand
TEST(PuzzleTrace, DISABLED_Print) {
    PhysicsRuntime runtime;
    for(const auto kind: AllPuzzleKinds) {
        for(const auto mode: AllPuzzleModes) {
            std::println("== {} {}", enumName(kind), enumName(mode));
            PuzzleSession session(runtime, kind, mode);
            const auto& puzzle = session.GetPuzzle();
            const auto& world = session.GetWorld();
            while(session.GetStatus().tick < PuzzleHorizon) {
                session.Tick();
                if(session.GetStatus().tick % 10 != 0)
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
                    const auto velocity =
                        world.LinearVelocityOf(session.HandleOf(i));
                    std::print(
                        "| {} ({:.3f} {:.3f} {:.3f}) v({:.2f} {:.2f} {:.2f}) "
                        "q({:.3f} {:.3f} {:.3f} {:.3f}) ",
                        puzzle.bodies[i].name,
                        pose.position.x,
                        pose.position.y,
                        pose.position.z,
                        velocity.x,
                        velocity.y,
                        velocity.z,
                        pose.rotation.x,
                        pose.rotation.y,
                        pose.rotation.z,
                        pose.rotation.w
                    );
                }
                for(u32 i = 0; i < puzzle.hinges.size(); ++i) {
                    const auto hinge = session.HingeHandleOf(i);
                    std::print(
                        "| hinge {:.3f}{}",
                        world.HingeAngleOf(hinge),
                        world.IsHingeHeld(hinge) ? "" : " cut"
                    );
                }
                std::println("");
            }
        }
    }
}
