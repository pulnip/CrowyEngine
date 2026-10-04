#include <cmath>
#include <optional>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"
#include "PhysicsRuntime.hpp"
#include "PuzzleRun.hpp"
#include "PuzzleSession.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 Scoop = 1;
    constexpr u32 Ball = 5;
    constexpr f32 BallRadius = 0.1f;
    // where the ball sits at rest, against the front wall
    constexpr Vec3 Seat{0.25f, 0.1f, 0.0f};
    // the two carries agree from here on
    constexpr u64 SameFrom = 120;

    // the ball where the scoop would see it: the open edge is its origin
    Vec3 inScoop(const PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto scoop = world.PoseOf(session.HandleOf(Scoop));
        const auto ball = world.PoseOf(session.HandleOf(Ball)).position;
        const auto toScoop =
            inverseRigid(modelMat(scoop.position, scoop.rotation, ones()));

        return static_cast<Vec3>(toScoop * toVec4(ball, 1.0f));
    }

    // one tick, and how much it changed the scoop's velocity
    f32 tickJolt(PuzzleSession& session) {
        const auto& world = session.GetWorld();
        const auto scoop = session.HandleOf(Scoop);
        const auto before = world.LinearVelocityOf(scoop);
        session.Tick();

        return norm(world.LinearVelocityOf(scoop) - before);
    }

    Vec3 ballPosition(const PuzzleSession& session) {
        return session.GetWorld().PoseOf(session.HandleOf(Ball)).position;
    }
}

TEST(CarriedScoop, SolutionKeepsTheBallSeated) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::CarriedScoop,
        PuzzleMode::Solution
    );

    // at most 1.2 m/s^2 either way, under the slope's hold of g tan 8
    // degrees, 1.38 m/s^2
    f32 jolt = 0.0f;
    while(session.GetStatus().tick < 210) {
        jolt = std::max(jolt, tickJolt(session));
        const auto seat = inScoop(session);
        const auto tick = session.GetStatus().tick;
        EXPECT_NEAR(seat.x, Seat.x, 0.02f) << "at tick " << tick;
        EXPECT_NEAR(seat.y, Seat.y, 0.02f) << "at tick " << tick;
        // the side walls leave it 5 cm either way
        EXPECT_LT(std::abs(seat.z), 0.06f) << "at tick " << tick;
    }
    EXPECT_LT(jolt, 1.25f / 60.0f);

    runTo(session, PuzzleHorizon);
    const auto& status = session.GetStatus();
    EXPECT_TRUE(status.solved);
    // poured from tick 240
    EXPECT_GT(status.solvedAt, 240u);
    EXPECT_LT(status.solvedAt, 340u);
    const auto rest = ballPosition(session);
    EXPECT_NEAR(rest.y, 0.55f + BallRadius, 0.02f);
    EXPECT_GT(rest.x, 0.4f + BallRadius - 0.02f);
    EXPECT_LT(rest.x, 1.0f - BallRadius + 0.02f);
    EXPECT_EQ(status.tick, PuzzleHorizon);
    EXPECT_EQ(
        session.GetWorld().BodyCount(),
        PuzzleBodyCounts[indexOf(PuzzleKind::CarriedScoop)]
    );
}

TEST(CarriedScoop, ControlJoltsTheBallOut) {
    PhysicsRuntime runtime;
    PuzzleSession session(
        runtime,
        PuzzleKind::CarriedScoop,
        PuzzleMode::Control
    );

    f32 jolt = 0.0f;
    std::optional<u64> outTheBack;
    std::optional<u64> onTheFloor;
    while(session.GetStatus().tick < PuzzleHorizon) {
        jolt = std::max(jolt, tickJolt(session));
        const auto tick = session.GetStatus().tick;
        if(!outTheBack && inScoop(session).x < 0.0f)
            outTheBack = tick;
        if(!onTheFloor && ballPosition(session).y < 0.2f)
            onTheFloor = tick;
    }
    // from rest to 1.2 m/s in one tick
    EXPECT_GT(jolt, 1.1f);
    ASSERT_TRUE(outTheBack.has_value());
    EXPECT_GT(*outTheBack, 75u);
    EXPECT_LT(*outTheBack, 115u);
    ASSERT_TRUE(onTheFloor.has_value());
    EXPECT_LT(*onTheFloor, 140u);

    EXPECT_FALSE(session.GetStatus().solved);
    // on the floor, short of the basket it cannot climb
    const auto rest = ballPosition(session);
    EXPECT_NEAR(rest.y, BallRadius, 0.02f);
    EXPECT_LT(rest.x, 0.35f - BallRadius + 0.02f);
    EXPECT_EQ(session.GetStatus().tick, PuzzleHorizon);
}

TEST(CarriedScoop, BothCarriesAgreeAfterTheStart) {
    PhysicsRuntime runtime;
    PuzzleSession solution(
        runtime,
        PuzzleKind::CarriedScoop,
        PuzzleMode::Solution
    );
    PuzzleSession control(
        runtime,
        PuzzleKind::CarriedScoop,
        PuzzleMode::Control
    );
    runTo(solution, SameFrom);
    runTo(control, SameFrom);
    u64 compared = 0;
    while(solution.GetStatus().tick < PuzzleHorizon) {
        const auto a = solution.GetWorld().PoseOf(solution.HandleOf(Scoop));
        const auto b = control.GetWorld().PoseOf(control.HandleOf(Scoop));
        EXPECT_LT(distance(a.position, b.position), 1.0e-4f)
            << "at tick " << solution.GetStatus().tick;
        solution.Tick();
        control.Tick();
        ++compared;
    }
    EXPECT_EQ(compared, PuzzleHorizon - SameFrom);
}
