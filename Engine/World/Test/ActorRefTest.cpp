#include <format>
#include <vector>

#include <gtest/gtest.h>

#include "ActorRef.hpp"
#include "ClassRegistry.hpp"
#include "JsonLoader.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    struct RefHolder {
        ActorRef target;
        std::vector<ActorRef> path;
    };

    DOM::Value serialized(const ActorRef& ref) {
        DOM::Value value;
        GetTypeOps<ActorRef>()->serialize(&ref, value);
        return value;
    }

    bool deserialized(ActorRef& ref, const DOM::Value& value) {
        return GetTypeOps<ActorRef>()->deserialize(&ref, value);
    }
}

namespace Crowy
{
    // clang-format off
    CROWY_STRUCT(RefHolder)
        .SetProperty("target", &RefHolder::target)
        .SetProperty("path", &RefHolder::path)
    CROWY_STRUCT_END(RefHolder)
    // clang-format on
}

TEST(ActorRef, AFileWritesAndReadsItsOwnIds) {
    World world;
    const auto a = world.Spawn();
    const auto b = world.Spawn();
    const RefHolder holder{.target = {b}, .path = {{a}, {b}, {}}};

    DOM::Value value;
    {
        ActorRefScope scope;
        scope.Bind(1, a);
        scope.Bind(2, b);
        SerializeProperties(&holder, value);
    }
    EXPECT_EQ(emitJson(value), R"({"target":2,"path":[1,2,null]})");

    // a load spawns its actors first, then binds the file's ids to them
    World other;
    const auto c = other.Spawn();
    const auto d = other.Spawn();
    RefHolder back;
    {
        ActorRefScope scope;
        scope.Bind(1, c);
        scope.Bind(2, d);
        EXPECT_TRUE(ApplyProperties(&back, value).empty());
    }
    EXPECT_EQ(back.target.actor, d);
    EXPECT_EQ(
        back.path,
        (std::vector<ActorRef>{{c}, {d}, {ActorId::InvalidHandle()}})
    );
}

TEST(ActorRef, AnIdTheFileNeverGaveIsRefused) {
    World world;
    ActorRefScope scope;
    scope.Bind(1, world.Spawn());

    RefHolder holder;
    const auto errors = ApplyProperties(
        &holder,
        parseJsonString(R"({"target":7,"path":[1,[0,0]]})")
    );
    EXPECT_EQ(
        errors,
        (PropertyErrors{
            "'path[1]' expects ActorRef",
            "'target' expects ActorRef"
        })
    );
}

// an actor the scope does not hold is none to the file
TEST(ActorRef, AnActorOutsideTheFileWritesAsNone) {
    World world;
    const auto stranger = world.Spawn();

    ActorRefScope scope;
    EXPECT_TRUE(serialized(ActorRef{stranger}).is_none());
}

TEST(ActorRef, OutsideAScopeItIsTheLiveHandle) {
    World world;
    world.Spawn();
    const ActorRef ref{world.Spawn()};

    const auto value = serialized(ref);
    EXPECT_EQ(
        emitJson(value),
        std::format("[{},{}]", ref.actor.GetIndex(), ref.actor.GetGeneration())
    );

    ActorRef back;
    ASSERT_TRUE(deserialized(back, value));
    EXPECT_EQ(back, ref);

    // a file's id means nothing here, and none is none
    EXPECT_FALSE(deserialized(back, DOM::Value(i64{1})));
    EXPECT_TRUE(serialized(ActorRef{}).is_none());
    ASSERT_TRUE(deserialized(back, DOM::Value()));
    EXPECT_FALSE(back.actor.IsValid());
}

TEST(ActorRef, AnInnerScopeHidesTheOuterUntilItEnds) {
    World world;
    const auto actor = world.Spawn();

    ActorRefScope outer;
    outer.Bind(1, actor);
    {
        ActorRefScope inner;
        EXPECT_EQ(ActorRefScope::Current(), &inner);
        EXPECT_TRUE(serialized(ActorRef{actor}).is_none());
    }
    EXPECT_EQ(ActorRefScope::Current(), &outer);
    EXPECT_EQ(serialized(ActorRef{actor}).asInt(), 1);
}
