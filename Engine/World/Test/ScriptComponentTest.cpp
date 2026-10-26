#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "FixedTickClock.hpp"
#include "ScriptComponent.hpp"
#include "System.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    struct ScriptCounter {
        i32 value = 0;
    };

    // one exact fixed tick, as a counted frame is given
    const f64 OneTick = static_cast<f64>(TickNanoseconds) * 1e-9;

    std::vector<i32> extractedCounts;

    void extractCounters(World& world, const FrameTime&) {
        world.EachChanged<ScriptCounter>([](ActorId,
                                            const ScriptCounter& counter) {
            extractedCounts.push_back(counter.value);
        });
    }

    ScriptComponent recordTicks(std::vector<u64>& ticks) {
        return ScriptComponent{
            [&ticks](World&, ActorId, const FrameTime& time) {
                ticks.push_back(time.tick);
            }
        };
    }
}

namespace Crowy
{
    // clang-format off
    CROWY_COMPONENT(ScriptCounter)
    CROWY_COMPONENT_END(ScriptCounter)

    CROWY_SYSTEM(ExtractScriptCounters, Extract, &extractCounters)
    CROWY_SYSTEM_END(ExtractScriptCounters)
    // clang-format on
}

// nothing here names a symbol of ScriptComponent.cpp, so only linking the
// module whole keeps its component and its system
TEST(ScriptComponent, RegistersWithoutAReference) {
    const auto* desc = ClassRegistry::FindType("ScriptComponent");
    ASSERT_TRUE(desc != nullptr);
    EXPECT_TRUE(ComponentRegistry::Get().Find(*desc) != nullptr);

    const auto& systems = SystemRegistry::Global().All();
    EXPECT_TRUE(std::ranges::any_of(systems, [](const SystemDesc& system) {
        return system.name == "RunScripts" && system.phase == Phase::Update;
    }));
}

// a function is no data a scene file could hold
TEST(ScriptComponent, IsLeftOutOfAScene) {
    const auto* info =
        ComponentRegistry::Get().Find(*GetDesc<ScriptComponent>());
    ASSERT_TRUE(info != nullptr);
    EXPECT_TRUE(info->transient);
}

// it lands at Sync, after the frame's Update, so it first runs a frame later
TEST(ScriptComponent, RunsOnceAFrameAfterTheFramesTicks) {
    std::vector<u64> ticks;
    World world;
    world.Add(world.Spawn(), recordTicks(ticks));

    world.Frame(OneTick);
    EXPECT_TRUE(ticks.empty());

    world.Frame(OneTick);
    world.Frame(2.0 * OneTick);
    world.Frame(0.0);
    EXPECT_EQ(ticks, (std::vector<u64>{2, 4, 4}));
}

TEST(ScriptComponent, ScriptsRunInTheOrderTheyLanded) {
    std::vector<i32> order;
    World world;
    for(i32 i = 0; i < 3; ++i) {
        world.Add(
            world.Spawn(),
            ScriptComponent{[&order, i](World&, ActorId, const FrameTime&) {
                order.push_back(i);
            }}
        );
    }

    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(order, (std::vector<i32>{0, 1, 2}));
}

// the removal at Sync moves the last script into the destroyed one's row
TEST(ScriptComponent, ARemovalMovesTheLastScriptIntoItsRow) {
    std::vector<i32> order;
    std::vector<ActorId> actors;
    World world;
    for(i32 i = 0; i < 3; ++i) {
        actors.push_back(world.Spawn());
        world.Add(
            actors.back(),
            ScriptComponent{[&order, i](World&, ActorId, const FrameTime&) {
                order.push_back(i);
            }}
        );
    }
    world.Frame(0.0);

    world.Destroy(actors[0]);
    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(order, (std::vector<i32>{1, 2, 2, 1}));
}

TEST(ScriptComponent, AScriptsWriteIsTheNextExtractsChange) {
    World world;
    const auto actor = world.Spawn();
    world.Add(actor, ScriptCounter{});
    world.Add(
        actor,
        ScriptComponent{[](World& world, ActorId self, const FrameTime&) {
            ++world.Write<ScriptCounter>(self)->value;
        }}
    );
    world.Frame(0.0);

    extractedCounts.clear();
    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(extractedCounts, (std::vector<i32>{1, 2}));
}

TEST(ScriptComponent, ADestroyedActorsScriptNeverRunsAgain) {
    std::vector<u64> ticks;
    World world;
    const auto actor = world.Spawn();
    world.Add(actor, recordTicks(ticks));
    world.Frame(0.0);
    world.Frame(0.0);
    ASSERT_EQ(ticks.size(), 1u);

    // its row stays until this frame's Sync, but the actor is already gone
    world.Destroy(actor);
    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(ticks.size(), 1u);
}

TEST(ScriptComponent, AScriptMayDestroyItsOwnActor) {
    i32 runs = 0;
    World world;
    const auto actor = world.Spawn();
    world.Add(
        actor,
        ScriptComponent{[&runs](World& world, ActorId self, const FrameTime&) {
            ++runs;
            world.Destroy(self);
        }}
    );
    world.Frame(0.0);

    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(runs, 1);
    EXPECT_FALSE(world.IsValid(actor));
}
