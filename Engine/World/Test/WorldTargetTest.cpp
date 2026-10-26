#include <vector>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "LinearAlgebra.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    struct TargetMarker {
        i32 value = 0;
    };

    // the marker values an Extract system sees changed, per frame
    std::vector<std::vector<i32>> extracted;

    void recordChanges(World& world, const FrameTime&) {
        auto& frame = extracted.emplace_back();
        world.EachChanged<TargetMarker>([&](ActorId,
                                            const TargetMarker& marker) {
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

    i32 valueOf(const PropertyTarget& target) {
        return static_cast<const TargetMarker*>(target.Read())->value;
    }

    const Transform* transformOf(const PropertyTarget& target) {
        return static_cast<const Transform*>(target.Read());
    }
}

namespace Crowy
{
    // clang-format off
    CROWY_COMPONENT(TargetMarker)
        .SetProperty("value", &TargetMarker::value)
    CROWY_COMPONENT_END(TargetMarker)
    // clang-format on
}

TEST(WorldTarget, FindsTheRowOnlyOnceItLands) {
    World world;
    const auto actor = world.Spawn();
    world.Add(actor, TargetMarker{.value = 3});
    const auto target = world.Target<TargetMarker>(actor);

    EXPECT_EQ(target.Read(), nullptr);
    world.Frame(0.0);
    EXPECT_EQ(valueOf(target), 3);
}

TEST(WorldTarget, FindsItsRowAgainAfterThePoolMovedIt) {
    World world;
    std::vector<ActorId> actors;
    for(i32 i = 0; i < 3; ++i) {
        actors.push_back(world.Spawn());
        world.Add(actors.back(), TargetMarker{.value = i});
    }
    world.Frame(0.0);
    const auto target = world.Target<TargetMarker>(actors[2]);

    // past the pool's first capacity, so its rows reallocate
    for(i32 i = 3; i < 40; ++i)
        world.Add(world.Spawn(), TargetMarker{.value = i});
    world.Frame(0.0);
    EXPECT_EQ(valueOf(target), 2);

    // the last row moves into the hole
    world.Remove<TargetMarker>(actors[0]);
    world.Frame(0.0);
    EXPECT_EQ(valueOf(target), 2);
}

TEST(WorldTarget, IsGoneOnceItsActorOrItsComponentIs) {
    World world;
    const auto destroyed = world.Spawn();
    const auto stripped = world.Spawn();
    world.Add(destroyed, TargetMarker{});
    world.Add(stripped, TargetMarker{});
    world.Frame(0.0);
    const auto destroyedTarget = world.Target<TargetMarker>(destroyed);
    const auto strippedTarget = world.Target<TargetMarker>(stripped);

    // the row stays until Sync, but the actor is gone already
    world.Destroy(destroyed);
    EXPECT_EQ(destroyedTarget.Read(), nullptr);
    EXPECT_EQ(destroyedTarget.Write(), nullptr);

    world.Remove<TargetMarker>(stripped);
    world.Frame(0.0);
    EXPECT_EQ(destroyedTarget.Read(), nullptr);
    EXPECT_EQ(strippedTarget.Read(), nullptr);
}

TEST(WorldTarget, AStaleIdNeverReachesTheActorThatReusedItsSlot) {
    World world;
    const auto old = world.Spawn();
    world.Add(old, TargetMarker{.value = 1});
    world.Frame(0.0);
    const auto oldTarget = world.Target<TargetMarker>(old);

    world.Destroy(old);
    world.Frame(0.0);
    const auto reused = world.Spawn();
    ASSERT_EQ(reused.GetIndex(), old.GetIndex());
    world.Add(reused, TargetMarker{.value = 2});
    world.Frame(0.0);

    EXPECT_EQ(oldTarget.Read(), nullptr);
    EXPECT_EQ(valueOf(world.Target<TargetMarker>(reused)), 2);
}

TEST(WorldTarget, OnlyAWriteAccessIsTheNextExtractsChange) {
    extracted.clear();
    const auto registry = changeRecorder();
    World world(registry);
    const auto actor = world.Spawn();
    world.Add(actor, TargetMarker{.value = 1});
    world.Frame(0.0);
    const auto target = world.Target<TargetMarker>(actor);

    target.Read();
    world.Frame(0.0);
    EXPECT_TRUE(extracted.back().empty());

    static_cast<TargetMarker*>(target.Write())->value = 4;
    world.Frame(0.0);
    EXPECT_EQ(extracted.back(), (std::vector<i32>{4}));
}

TEST(WorldTarget, ATransformWriteMovesTheActorAndThoseBelowIt) {
    extracted.clear();
    const auto registry = changeRecorder();
    World world(registry);
    const auto parent = world.Spawn();
    const auto child = world.Spawn(Transform::Identity(), parent);
    const auto other = world.Spawn();
    world.Add(parent, TargetMarker{.value = 1});
    world.Add(child, TargetMarker{.value = 2});
    world.Add(other, TargetMarker{.value = 3});
    world.Frame(0.0);

    const auto target = world.TransformTarget(parent);
    static_cast<Transform*>(target.Write())->position = {0.0f, 2.0f, 0.0f};
    world.Frame(0.0);

    EXPECT_EQ(world.WorldPosition(child), (Vec3{0.0f, 2.0f, 0.0f}));
    EXPECT_EQ(extracted.back(), (std::vector<i32>{1, 2}));
}

TEST(WorldTarget, ATransformWrittenBeforeTheFirstSyncLandsThere) {
    World world;
    const auto actor = world.Spawn(Transform{.position = {1.0f, 0.0f, 0.0f}});
    const auto target = world.TransformTarget(actor);
    EXPECT_EQ(transformOf(target)->position, (Vec3{1.0f, 0.0f, 0.0f}));

    static_cast<Transform*>(target.Write())->position = {2.0f, 0.0f, 0.0f};
    world.Frame(0.0);
    EXPECT_EQ(world.WorldPosition(actor), (Vec3{2.0f, 0.0f, 0.0f}));
}

TEST(WorldTarget, ATransformTargetIsGoneWithItsActor) {
    World world;
    const auto actor = world.Spawn();
    const auto target = world.TransformTarget(actor);

    world.Destroy(actor);
    EXPECT_EQ(target.Read(), nullptr);
}

// registered in World.cpp, which this binary links only for World itself
TEST(WorldTarget, TheTransformIsAReflectedStruct) {
    World world;
    const auto target = world.TransformTarget(world.Spawn());
    EXPECT_EQ(target.Desc().name, "Transform");

    Transform transform;
    const auto rotation =
        ResolveProperty(&transform, target.Desc(), "rotation");
    ASSERT_NE(rotation.type, nullptr) << rotation.error;
    EXPECT_EQ(rotation.type, GetTypeOps<Vec4>());
}

// what an Extract reads for a camera or a light, to the bit
TEST(WorldTarget, ARootsWorldPoseIsItsLocalPose) {
    World world;
    const auto rotation = quat(rotateY(0.3f), rotateX(-0.2f));
    const Vec3 position{1.5f, -2.0f, 3.25f};
    const auto parent =
        world.Spawn(Transform{.position = position, .rotation = rotation});
    const auto turn = rotateZ(0.7f);
    const auto child = world.Spawn(Transform{.rotation = turn}, parent);
    world.Frame(0.0);

    EXPECT_EQ(world.WorldPosition(parent), position);
    EXPECT_EQ(world.WorldRotation(parent), rotation);
    EXPECT_EQ(world.WorldRotation(child), quat(rotation, turn));
}

#if defined(_DEBUG) || !defined(NDEBUG)
TEST(WorldTargetDeathTest, AWriteAccessInsideExtractStops) {
    SystemRegistry registry;
    registry.Add(
        SystemDesc{
            .name = "writer",
            .phase = Phase::Extract,
            .run = +[](World& world, const FrameTime&) {
                world.Each<TargetMarker>([&](ActorId actor,
                                             const TargetMarker&) {
                    world.Target<TargetMarker>(actor).Write();
                });
            }
        }
    );
    World world(registry);
    world.Add(world.Spawn(), TargetMarker{});

    EXPECT_DEATH(world.Frame(0.0), "a component written inside Extract");
}
#endif
