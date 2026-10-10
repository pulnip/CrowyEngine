#include <format>
#include <memory>
#include <vector>

#include <gtest/gtest.h>

#include "Component.hpp"
#include "Object.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    struct WorldMarker {
        i32 value = 0;
    };

    struct WorldOwned {
        std::unique_ptr<i32> value;
    };

    struct WorldCounted {
        static inline i32 alive = 0;

        WorldCounted() { ++alive; }
        ~WorldCounted() { --alive; }
        WorldCounted(const WorldCounted&) { ++alive; }
        WorldCounted& operator=(const WorldCounted&) = default;
        WorldCounted(WorldCounted&&) noexcept { ++alive; }
        WorldCounted& operator=(WorldCounted&&) noexcept = default;
    };

    struct WorldHooked {
        i32 value = 0;
    };

    struct WorldPlainStruct {
        i32 value = 0;
    };

    std::vector<Str> hookCalls;

    void onHookedAdd(World&, ActorId, WorldHooked& hooked) {
        hookCalls.push_back(std::format("add {}", hooked.value));
    }

    void onHookedRemove(World&, ActorId, WorldHooked& hooked) {
        hookCalls.push_back(std::format("remove {}", hooked.value));
    }

    Transform at(Vec3 position) {
        return Transform{.position = position};
    }

    std::vector<i32> markerValues(World& world) {
        std::vector<i32> values;
        world.Each<WorldMarker>([&](ActorId, const WorldMarker& marker) {
            values.push_back(marker.value);
        });

        return values;
    }
}

namespace Crowy
{
    // clang-format off: each registration opens a namespace the formatter
    // cannot see
    CROWY_COMPONENT(WorldMarker)
        .SetProperty("value", &WorldMarker::value)
    CROWY_COMPONENT_END(WorldMarker)

    CROWY_COMPONENT(WorldOwned)
    CROWY_COMPONENT_END(WorldOwned)

    CROWY_COMPONENT(WorldCounted)
    CROWY_COMPONENT_END(WorldCounted)

    CROWY_COMPONENT(WorldHooked)
        .OnAdd(&onHookedAdd)
        .OnRemove(&onHookedRemove)
    CROWY_COMPONENT_END(WorldHooked)

    CROWY_STRUCT(WorldPlainStruct)
        .SetProperty("value", &WorldPlainStruct::value)
    CROWY_STRUCT_END(WorldPlainStruct)
    // clang-format on
}

TEST(World, ASpawnIsAliveAtOnceAndPlacedAtSync) {
    World world;
    const auto actor = world.Spawn(at({1.0f, 2.0f, 3.0f}));
    EXPECT_TRUE(world.IsValid(actor));

    world.Frame(0.0);
    EXPECT_EQ(world.WorldMatrix(actor)[3], (Vec4{1.0f, 2.0f, 3.0f, 1.0f}));
}

TEST(World, AChildIsPlacedUnderItsParent) {
    World world;
    const auto parent = world.Spawn(at({1.0f, 0.0f, 0.0f}));
    const auto child = world.Spawn(at({0.0f, 2.0f, 0.0f}), parent);

    world.Frame(0.0);
    EXPECT_EQ(world.WorldMatrix(child)[3], (Vec4{1.0f, 2.0f, 0.0f, 1.0f}));
}

TEST(World, ADestroyTakesTheSubtreeAtOnce) {
    World world;
    const auto parent = world.Spawn();
    const auto child = world.Spawn(Transform::Identity(), parent);
    const auto other = world.Spawn();
    world.Frame(0.0);

    world.Destroy(parent);
    EXPECT_FALSE(world.IsValid(parent));
    EXPECT_FALSE(world.IsValid(child));
    EXPECT_TRUE(world.IsValid(other));

    world.Frame(0.0);
    // the freed slots go to new actors, and the old handles stay dead
    world.Spawn();
    world.Spawn();
    EXPECT_FALSE(world.IsValid(parent));
    EXPECT_FALSE(world.IsValid(child));
}

TEST(World, ComponentsLandAtSyncInTheOrderTheyWereAdded) {
    World world;
    std::vector<ActorId> actors;
    for(i32 i = 1; i <= 3; ++i) {
        actors.push_back(world.Spawn());
        world.Add(actors.back(), WorldMarker{.value = i});
    }
    EXPECT_TRUE(world.Get<WorldMarker>(actors[0]) == nullptr);

    world.Frame(0.0);
    EXPECT_EQ(markerValues(world), (std::vector<i32>{1, 2, 3}));
    EXPECT_EQ(world.Get<WorldMarker>(actors[1])->value, 2);
}

TEST(World, ARemovalMovesTheLastRowIntoTheHole) {
    World world;
    std::vector<ActorId> actors;
    for(i32 i = 1; i <= 3; ++i) {
        actors.push_back(world.Spawn());
        world.Add(actors.back(), WorldMarker{.value = i});
    }
    world.Frame(0.0);

    world.Remove<WorldMarker>(actors[0]);
    // still there until Sync
    EXPECT_EQ(world.Get<WorldMarker>(actors[0])->value, 1);

    world.Frame(0.0);
    EXPECT_TRUE(world.Get<WorldMarker>(actors[0]) == nullptr);
    EXPECT_EQ(markerValues(world), (std::vector<i32>{3, 2}));
    EXPECT_EQ(world.Get<WorldMarker>(actors[2])->value, 3);
}

TEST(World, AMoveOnlyComponentSurvivesThePoolGrowing) {
    World world;
    std::vector<ActorId> actors;
    for(i32 i = 0; i < 100; ++i) {
        actors.push_back(world.Spawn());
        world.Add(actors.back(), WorldOwned{std::make_unique<i32>(i)});
    }
    world.Frame(0.0);

    for(i32 i = 0; i < 100; ++i)
        EXPECT_EQ(*world.Get<WorldOwned>(actors[i])->value, i);
}

TEST(World, EveryComponentDiesWithItsActorOrItsWorld) {
    WorldCounted::alive = 0;
    {
        World world;
        std::vector<ActorId> actors;
        for(i32 i = 0; i < 20; ++i) {
            actors.push_back(world.Spawn());
            world.Add(actors.back(), WorldCounted{});
        }
        world.Frame(0.0);
        EXPECT_EQ(WorldCounted::alive, 20);

        for(i32 i = 0; i < 5; ++i)
            world.Destroy(actors[i]);
        world.Frame(0.0);
        EXPECT_EQ(WorldCounted::alive, 15);
    }
    EXPECT_EQ(WorldCounted::alive, 0);
}

TEST(World, AComponentAddedToAnActorDestroyedThatFrameNeverLands) {
    WorldCounted::alive = 0;
    World world;
    const auto actor = world.Spawn();
    world.Add(actor, WorldMarker{.value = 1});
    world.Add(actor, WorldCounted{});
    world.Destroy(actor);

    world.Frame(0.0);
    EXPECT_TRUE(markerValues(world).empty());
    EXPECT_EQ(WorldCounted::alive, 0);
}

TEST(World, HooksRunAtSyncAroundTheRow) {
    hookCalls.clear();
    World world;
    const auto kept = world.Spawn();
    const auto destroyed = world.Spawn();
    world.Add(kept, WorldHooked{.value = 7});
    world.Add(destroyed, WorldHooked{.value = 8});
    EXPECT_TRUE(hookCalls.empty());

    world.Frame(0.0);
    EXPECT_EQ(hookCalls, (std::vector<Str>{"add 7", "add 8"}));

    world.Remove<WorldHooked>(kept);
    world.Destroy(destroyed);
    world.Frame(0.0);
    // the dying leave before the removals
    EXPECT_EQ(
        hookCalls,
        (std::vector<Str>{"add 7", "add 8", "remove 8", "remove 7"})
    );
}

namespace
{
    // the marker values an Extract system sees changed, per frame
    std::vector<std::vector<i32>> extracted;

    void recordChanges(World& world, const FrameTime&) {
        auto& frame = extracted.emplace_back();
        world.EachChanged<WorldMarker>([&](ActorId, const WorldMarker& marker) {
            frame.push_back(marker.value);
        });
    }

    SystemRegistry changeRecorder() {
        SystemRegistry registry;
        registry.Add(
            SystemDesc{
                .name = "recordChanges",
                .phase = Phase::Extract,
                .run = &recordChanges
            }
        );

        return registry;
    }
}

TEST(WorldChanges, ARowIsChangedTheFrameItLandsAndQuietAfter) {
    extracted.clear();
    const auto registry = changeRecorder();
    World world(registry);
    for(i32 i = 1; i <= 3; ++i)
        world.Add(world.Spawn(), WorldMarker{.value = i});

    world.Frame(0.0);
    world.Frame(0.0);
    EXPECT_EQ(extracted, (std::vector<std::vector<i32>>{{1, 2, 3}, {}}));
}

TEST(WorldChanges, AWriteIsTheNextExtractsChange) {
    extracted.clear();
    const auto registry = changeRecorder();
    World world(registry);
    const auto kept = world.Spawn();
    const auto written = world.Spawn();
    world.Add(kept, WorldMarker{.value = 1});
    world.Add(written, WorldMarker{.value = 2});
    world.Frame(0.0);

    world.Write<WorldMarker>(written)->value = 5;
    // written twice, reported once
    world.Write<WorldMarker>(written)->value = 6;
    world.Frame(0.0);

    EXPECT_EQ(extracted.back(), (std::vector<i32>{6}));
    EXPECT_EQ(world.Get<WorldMarker>(written)->value, 6);
}

TEST(WorldChanges, AMovedActorChangesItsRowsAndThoseBelowIt) {
    extracted.clear();
    const auto registry = changeRecorder();
    World world(registry);
    const auto parent = world.Spawn();
    const auto child = world.Spawn(Transform::Identity(), parent);
    const auto other = world.Spawn();
    world.Add(parent, WorldMarker{.value = 1});
    world.Add(child, WorldMarker{.value = 2});
    world.Add(other, WorldMarker{.value = 3});
    world.Frame(0.0);

    world.SetLocalTransform(parent, Transform{.position = {1.0f, 0.0f, 0.0f}});
    world.Frame(0.0);
    EXPECT_EQ(extracted.back(), (std::vector<i32>{1, 2}));
}

TEST(WorldChanges, AWriteToAnActorDestroyedThatFrameIsNotReported) {
    extracted.clear();
    const auto registry = changeRecorder();
    World world(registry);
    const auto actor = world.Spawn();
    world.Add(actor, WorldMarker{.value = 1});
    world.Frame(0.0);

    world.Write<WorldMarker>(actor)->value = 2;
    world.Destroy(actor);
    world.Frame(0.0);
    EXPECT_TRUE(extracted.back().empty());
}

TEST(World, AProvidedServiceIsFoundByItsType) {
    World world;
    i32 counter = 4;
    world.Provide(counter);

    EXPECT_EQ(&world.Service<i32>(), &counter);
}

#if defined(_DEBUG) || !defined(NDEBUG)
TEST(WorldDeathTest, AStructThatIsNotAComponentStops) {
    World world;
    const auto actor = world.Spawn();

    EXPECT_DEATH(
        world.Add(actor, WorldPlainStruct{}),
        "a type that is not a component"
    );
}

TEST(WorldDeathTest, ATypeNeverRegisteredStops) {
    struct NeverRegistered {
        i32 value = 0;
    };
    World world;
    const auto actor = world.Spawn();

    EXPECT_DEATH(
        world.Add(actor, NeverRegistered{}),
        "a type that was never registered"
    );
}

TEST(WorldDeathTest, WritingAComponentInsideExtractStops) {
    SystemRegistry registry;
    registry.Add(
        SystemDesc{
            .name = "writer",
            .phase = Phase::Extract,
            .run = +[](World& world, const FrameTime&) {
                world.Each<WorldMarker>([&](ActorId actor, const WorldMarker&) {
                    world.Write<WorldMarker>(actor);
                });
            }
        }
    );
    World world(registry);
    world.Add(world.Spawn(), WorldMarker{});

    EXPECT_DEATH(world.Frame(0.0), "a component written inside Extract");
}

TEST(WorldDeathTest, AddingAComponentTwiceStops) {
    World world;
    const auto actor = world.Spawn();
    world.Add(actor, WorldMarker{});

    EXPECT_DEATH(
        world.Add(actor, WorldMarker{}),
        "the actor already has this component"
    );
}
#endif
