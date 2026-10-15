#include <format>
#include <memory>
#include <unordered_map>
#include <utility>
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

    using Walk = std::vector<std::pair<ActorId, ActorId>>;

    Walk walk(const World& world) {
        Walk visits;
        world.EachActor([&](ActorId actor, ActorId parent) {
            visits.emplace_back(actor, parent);
        });

        return visits;
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

TEST(World, EveryActorIsWalkedParentFirst) {
    World world;
    const auto first = world.Spawn();
    const auto second = world.Spawn();
    const auto child = world.Spawn(Transform::Identity(), first);
    const auto secondChild = world.Spawn(Transform::Identity(), second);
    const auto grandchild = world.Spawn(Transform::Identity(), child);
    world.Frame(0.0);

    const auto none = ActorId::InvalidHandle();
    EXPECT_EQ(
        walk(world),
        (Walk{
            {first, none},
            {child, first},
            {grandchild, child},
            {second, none},
            {secondChild, second}
        })
    );
}

TEST(World, ActorsSpawnedInWalkOrderWalkTheSameWay) {
    World source;
    const auto first = source.Spawn();
    const auto second = source.Spawn();
    const auto child = source.Spawn(Transform::Identity(), first);
    source.Spawn(Transform::Identity(), second);
    source.Spawn(Transform::Identity(), child);
    source.Frame(0.0);

    World copy;
    std::unordered_map<ActorId, ActorId> copied;
    for(const auto& [actor, parent]: walk(source)) {
        const auto copiedParent =
            parent.IsValid() ? copied.at(parent) : ActorId::InvalidHandle();
        copied[actor] = copy.Spawn(Transform::Identity(), copiedParent);
    }
    copy.Frame(0.0);

    Walk expected;
    for(const auto& [actor, parent]: walk(source)) {
        expected.emplace_back(
            copied.at(actor),
            parent.IsValid() ? copied.at(parent) : ActorId::InvalidHandle()
        );
    }
    EXPECT_EQ(walk(copy), expected);
}

TEST(World, AnActorsComponentsAreVisitedByTheirDesc) {
    World world;
    const auto both = world.Spawn();
    const auto marked = world.Spawn();
    const auto bare = world.Spawn();
    world.Add(both, WorldMarker{.value = 1});
    world.Add(both, WorldOwned{});
    world.Add(marked, WorldMarker{.value = 2});
    world.Frame(0.0);

    auto visit = [&](ActorId actor) {
        std::vector<Str> seen;
        world.EachComponent(
            actor,
            [&](const ComponentInfo& info, const void* row) {
                auto entry = Str(info.desc->name);
                if(info.desc == GetDesc<WorldMarker>()) {
                    const auto value =
                        static_cast<const WorldMarker*>(row)->value;
                    entry += std::format(" {}", value);
                }
                seen.push_back(entry);
            }
        );

        return seen;
    };
    EXPECT_EQ(visit(both), (std::vector<Str>{"WorldMarker 1", "WorldOwned"}));
    EXPECT_EQ(visit(marked), (std::vector<Str>{"WorldMarker 2"}));
    EXPECT_TRUE(visit(bare).empty());
}

TEST(World, AnActorIsNamedOnceItIsAlive) {
    World world;
    const auto first = world.Spawn();
    const auto second = world.Spawn();
    EXPECT_EQ(world.Name(first), "");

    world.SetName(first, "lamp");
    world.SetName(second, "lamp");
    EXPECT_EQ(world.Name(first), "lamp");
    EXPECT_EQ(world.Name(second), "lamp");

    world.SetName(second, "post");
    EXPECT_EQ(world.Name(first), "lamp");
    EXPECT_EQ(world.Name(second), "post");
}

TEST(World, AReusedSlotStartsUnnamed) {
    World world;
    const auto old = world.Spawn();
    world.SetName(old, "lamp");
    world.Frame(0.0);

    world.Destroy(old);
    world.Frame(0.0);
    const auto reused = world.Spawn();
    ASSERT_EQ(reused.GetIndex(), old.GetIndex());
    EXPECT_EQ(world.Name(reused), "");
}

TEST(World, EverySpawnTakesTheNextFileId) {
    World world;
    const auto first = world.Spawn();
    const auto second = world.Spawn(Transform::Identity(), first);

    EXPECT_EQ(world.FileId(first), 1);
    EXPECT_EQ(world.FileId(second), 2);
}

TEST(World, AGivenFileIdPutsLaterSpawnsAboveIt) {
    World world;
    const auto loaded = world.Spawn();
    world.SetFileId(loaded, 10);
    const auto next = world.Spawn();
    EXPECT_EQ(world.FileId(next), 11);

    // a lower id given later leaves the next spawn above the highest
    world.SetFileId(next, 3);
    EXPECT_EQ(world.FileId(world.Spawn()), 12);
}

TEST(World, AReusedSlotTakesANewFileId) {
    World world;
    const auto gone = world.Spawn();
    world.Frame(0.0);
    world.Destroy(gone);
    world.Frame(0.0);

    const auto reused = world.Spawn();
    ASSERT_EQ(reused.GetIndex(), gone.GetIndex());
    EXPECT_EQ(world.FileId(reused), 2);
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

TEST(World, AComponentAddedAsAnErasedValueIsMovedIn) {
    World world;
    const auto actor = world.Spawn();
    ErasedValue erased(*GetDesc<WorldOwned>()->ops);
    auto& owned = *static_cast<WorldOwned*>(erased.Get());
    owned.value = std::make_unique<i32>(7);

    world.Add(actor, erased);
    EXPECT_EQ(owned.value, nullptr);

    world.Frame(0.0);
    EXPECT_EQ(*world.Get<WorldOwned>(actor)->value, 7);
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

TEST(WorldDeathTest, AStructThatIsNotAComponentStopsAsAnErasedValueToo) {
    World world;
    const auto actor = world.Spawn();
    ErasedValue plain(*GetDesc<WorldPlainStruct>()->ops);

    EXPECT_DEATH(world.Add(actor, plain), "a type that is not a component");
}

TEST(WorldDeathTest, ALeafTypeStopsAsAnErasedValue) {
    World world;
    const auto actor = world.Spawn();
    ErasedValue leaf(*GetTypeOps<i32>());

    EXPECT_DEATH(world.Add(actor, leaf), "a type that is not a component");
}

TEST(WorldDeathTest, WalkingTheActorsBeforeSyncStops) {
    World world;
    world.Spawn();

    EXPECT_DEATH(
        walk(world),
        "walking the actors before Sync applied a change"
    );
}

TEST(WorldDeathTest, AFileIdBelowOneStops) {
    World world;
    const auto actor = world.Spawn();

    EXPECT_DEATH(world.SetFileId(actor, 0), "a file id starts at 1");
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
