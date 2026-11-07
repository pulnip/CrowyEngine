#include <gtest/gtest.h>

#include "ModelActors.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    ModelNode nodeAt(Str name, u32 parent, Vec3 position) {
        return ModelNode{
            .name = std::move(name),
            .parent = parent,
            .local = Transform{.position = position}
        };
    }
}

// a node's actor hangs under its own parent's actor, not the one before it,
// and every top node under the actor the model was spawned at
TEST(ModelActors, EachNodeHangsUnderItsParentsActor) {
    constexpr auto Top = ModelNode::NoParent;

    const ModelData model{
        .nodes = {
            nodeAt("Base", Top, Vec3{1.0f, 0.0f, 0.0f}),
            nodeAt("Arm", 0, Vec3{0.0f, 2.0f, 0.0f}),
            nodeAt("Tip", 1, Vec3{0.0f, 0.0f, 3.0f}),
            nodeAt("Lid", 0, Vec3{0.0f, 0.0f, 4.0f}),
            nodeAt("Stand", Top, Vec3{0.0f, 5.0f, 0.0f}),
        }
    };

    const SystemRegistry noSystems;
    World world{noSystems};
    const auto anchor = world.Spawn(Transform{.position = {10.0f, 0.0f, 0.0f}});
    const auto actors = spawnModelActors(world, model, anchor);
    world.Frame(0.0);

    ASSERT_EQ(actors.size(), model.nodes.size());
    EXPECT_EQ(world.Parent(actors[0]), anchor);
    EXPECT_EQ(world.Parent(actors[1]), actors[0]);
    EXPECT_EQ(world.Parent(actors[2]), actors[1]);
    EXPECT_EQ(world.Parent(actors[3]), actors[0]);
    EXPECT_EQ(world.Parent(actors[4]), anchor);

    for(usize i = 0; i < actors.size(); ++i) {
        EXPECT_EQ(world.Name(actors[i]), model.nodes[i].name);
        EXPECT_EQ(
            world.LocalTransform(actors[i]).position,
            model.nodes[i].local.position
        );
    }
    EXPECT_EQ(world.WorldPosition(actors[2]), (Vec3{11.0f, 2.0f, 3.0f}));
    EXPECT_EQ(world.WorldPosition(actors[3]), (Vec3{11.0f, 0.0f, 4.0f}));
}
