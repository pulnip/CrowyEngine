#include <vector>

#include <gtest/gtest.h>

#include "FixedTickClock.hpp"
#include "System.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    std::vector<Str> ran;
    std::vector<u64> ticksSeen;
    i32 globalRuns = 0;

    // one exact fixed tick, as a counted frame is given
    const f64 OneTick = static_cast<f64>(TickNanoseconds) * 1e-9;

    SystemDesc describe(
        Str name,
        Phase phase,
        SystemFn run,
        SystemNames after = {}
    ) {
        return SystemDesc{
            .name = std::move(name),
            .phase = phase,
            .run = run,
            .after = std::move(after)
        };
    }

    void countGlobalRuns(World&, const FrameTime&) {
        ++globalRuns;
    }
}

// a captureless lambda that records NAME when its system runs
#define RECORD(NAME)                                                           \
    +[](World&, const FrameTime&) {                                            \
        ran.push_back(NAME);                                                   \
    }

namespace Crowy
{
    // clang-format off
    CROWY_SYSTEM(WorldTestCounter, Update, &countGlobalRuns)
    CROWY_SYSTEM_END(WorldTestCounter)
    // clang-format on
}

TEST(WorldSystems, ThePhasesRunInTheirFixedOrder) {
    ran.clear();
    SystemRegistry registry;
    registry.Add(describe("extract", Phase::Extract, RECORD("Extract")));
    registry.Add(describe("sync", Phase::Sync, RECORD("Sync")));
    registry.Add(describe("update", Phase::Update, RECORD("Update")));
    registry.Add(describe(
        "postFixedUpdate",
        Phase::PostFixedUpdate,
        RECORD("PostFixedUpdate")
    ));
    registry.Add(describe("physics", Phase::Physics, RECORD("Physics")));
    registry.Add(describe(
        "preFixedUpdate",
        Phase::PreFixedUpdate,
        RECORD("PreFixedUpdate")
    ));
    registry.Add(describe("input", Phase::Input, RECORD("Input")));

    World world(registry);
    world.Frame(OneTick);

    EXPECT_EQ(
        ran,
        (std::vector<Str>{
            "Input",
            "PreFixedUpdate",
            "Physics",
            "PostFixedUpdate",
            "Update",
            "Sync",
            "Extract"
        })
    );
}

TEST(WorldSystems, AfterOrdersAPhaseAndNamesBreakTheTies) {
    ran.clear();
    SystemRegistry registry;
    registry.Add(describe("zeta", Phase::Update, RECORD("zeta")));
    registry.Add(describe("alpha", Phase::Update, RECORD("alpha"), {"zeta"}));
    registry.Add(describe("beta", Phase::Update, RECORD("beta")));

    World world(registry);
    world.Frame(0.0);

    EXPECT_EQ(ran, (std::vector<Str>{"beta", "zeta", "alpha"}));
}

TEST(WorldSystems, TheFixedUpdatePhasesRunOncePerTickTheGameClockOwes) {
    ticksSeen.clear();
    SystemRegistry registry;
    registry.Add(describe(
        "fixedUpdate",
        Phase::PreFixedUpdate,
        +[](World&, const FrameTime& time) { ticksSeen.push_back(time.tick); }
    ));
    World world(registry);

    // under a tick owes none, and the carry makes the next one whole
    world.Frame(OneTick * 0.5);
    EXPECT_TRUE(ticksSeen.empty());
    world.Frame(OneTick * 0.5);
    EXPECT_EQ(ticksSeen, (std::vector<u64>{1}));

    // a counted frame is one tick, and a long frame stops at the limit
    world.Frame(OneTick);
    EXPECT_EQ(ticksSeen, (std::vector<u64>{1, 2}));
    world.Frame(1.0);
    EXPECT_EQ(ticksSeen.size(), 2u + MaxTicksPerAdvance);
    EXPECT_EQ(ticksSeen.back(), 6u);
}

TEST(WorldSystems, TheWorldTellsItsTicksAndTheLastFramesTicks) {
    SystemRegistry registry;
    World world(registry);
    EXPECT_EQ(world.Ticks(), 0u);
    EXPECT_EQ(world.FrameTicks(), 0u);

    world.Frame(2.0 * OneTick);
    EXPECT_EQ(world.Ticks(), 2u);
    EXPECT_EQ(world.FrameTicks(), 2u);

    world.Frame(0.0);
    EXPECT_EQ(world.Ticks(), 2u);
    EXPECT_EQ(world.FrameTicks(), 0u);

    world.Frame(1.0);
    EXPECT_EQ(world.Ticks(), 2u + MaxTicksPerAdvance);
    EXPECT_EQ(world.FrameTicks(), MaxTicksPerAdvance);
}

TEST(WorldSystems, AnUnorderedRegistryStillRunsEachPhaseOnce) {
    ran.clear();
    SystemRegistry registry;
    registry.Add(
        describe("fixedUpdate", Phase::PreFixedUpdate, RECORD("fixedUpdate"))
    );
    registry.Add(describe("update", Phase::Update, RECORD("update")));

    World world(registry);
    world.Frame(0.0);

    // no tick is owed, so only the frame's phases run
    EXPECT_EQ(ran, (std::vector<Str>{"update"}));
}

TEST(WorldSystems, ASystemRegisteredByTheMacroRunsInADefaultWorld) {
    globalRuns = 0;
    World world;
    world.Frame(0.0);

    EXPECT_EQ(globalRuns, 1);
}

TEST(WorldSystemsDeathTest, ACycleStops) {
    SystemRegistry registry;
    registry.Add(describe("a", Phase::Update, RECORD("a"), {"b"}));
    registry.Add(describe("b", Phase::Update, RECORD("b"), {"a"}));
    registry.Add(describe("c", Phase::Update, RECORD("c")));

    EXPECT_DEATH(World{registry}, "systems in a cycle: a, b");
}

TEST(WorldSystemsDeathTest, AnUnknownNameStops) {
    SystemRegistry registry;
    registry.Add(describe("a", Phase::Update, RECORD("a"), {"missing"}));

    EXPECT_DEATH(
        World{registry},
        "'a' runs after 'missing', which is no system"
    );
}

TEST(WorldSystemsDeathTest, ANameInAnotherPhaseStops) {
    SystemRegistry registry;
    registry.Add(describe("a", Phase::Update, RECORD("a"), {"b"}));
    registry.Add(describe("b", Phase::Extract, RECORD("b")));

    EXPECT_DEATH(
        World{registry},
        "'a' runs after 'b', which runs in another phase"
    );
}

TEST(WorldSystemsDeathTest, ANameTakenTwiceStops) {
    SystemRegistry registry;
    registry.Add(describe("a", Phase::Update, RECORD("a")));

    EXPECT_DEATH(
        registry.Add(describe("a", Phase::Extract, RECORD("a"))),
        "two systems are named 'a'"
    );
}
