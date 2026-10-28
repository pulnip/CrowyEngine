#include <cmath>
#include <numbers>

#include <gtest/gtest.h>

#include "LinearAlgebra.hpp"
#include "PairedHierarchy.hpp"
#include "Primitives.hpp"
#include "TransformHierarchyInvariants.hpp"

using namespace Crowy;

namespace
{
    constexpr f32 EPSILON = 1e-5f;

    Transform scaled(Vec3 scale) {
        return Transform{.scale = scale};
    }

    Transform rotationZ(f32 radian) {
        return Transform{
            .rotation =
                Vec4{0.0f, 0.0f, std::sin(radian / 2), std::cos(radian / 2)}
        };
    }

}

class WorldAccessorTest: public ::testing::Test {
protected:
    TransformHierarchy hierarchy;

    TransformHandle Create(const Transform& local) {
        auto handle = hierarchy.CreateNode(local);
        hierarchy.CommitStructuralChanges();

        return handle;
    }
    TransformHandle Create(TransformHandle parent, const Transform& local) {
        auto handle = hierarchy.CreateNode(parent, local);
        hierarchy.CommitStructuralChanges();

        return handle;
    }

    u32 FlagsAt(usize index) const {
        return MakeView(hierarchy).nodes[index].flags;
    }
    static bool HasUnevenChain(u32 flags) {
        return (flags & TransformNode::NON_UNIFORM_IN_CHAIN) != 0;
    }
    static bool HasWarned(u32 flags) {
        return (flags & TransformNode::WARNED_NON_UNIFORM) != 0;
    }
};

TEST_F(WorldAccessorTest, WorldPosition) {
    auto root = Create(Transform{.position = Vec3{1.0f, 2.0f, 3.0f}});
    auto child = Create(root, Transform{.position = Vec3{0.0f, 1.0f, 0.0f}});

    hierarchy.UpdateWorldTransforms();

    auto position = hierarchy.GetWorldPosition(child);
    EXPECT_NEAR(position.x, 1.0f, EPSILON);
    EXPECT_NEAR(position.y, 3.0f, EPSILON);
    EXPECT_NEAR(position.z, 3.0f, EPSILON);
}

TEST_F(WorldAccessorTest, WorldRotationComposesTheChain) {
    constexpr f32 QUARTER = std::numbers::pi_v<f32> / 2;

    auto root = Create(rotationZ(QUARTER));
    auto child = Create(root, rotationZ(QUARTER));

    hierarchy.UpdateWorldTransforms();

    // two quarter turns about +Z make a half turn
    auto rotation = hierarchy.GetWorldRotation(child);
    auto expected = Vec4{0.0f, 0.0f, std::sin(QUARTER), std::cos(QUARTER)};
    EXPECT_NEAR(std::abs(dot(rotation, expected)), 1.0f, EPSILON);
}

TEST_F(WorldAccessorTest, UniformScaleDoesNotDisturbTheRotation) {
    constexpr f32 QUARTER = std::numbers::pi_v<f32> / 2;

    auto local = rotationZ(QUARTER);
    local.scale = Vec3{4.0f, 4.0f, 4.0f};
    auto root = Create(local);

    hierarchy.UpdateWorldTransforms();

    auto rotation = hierarchy.GetWorldRotation(root);
    auto expected =
        Vec4{0.0f, 0.0f, std::sin(QUARTER / 2), std::cos(QUARTER / 2)};
    EXPECT_NEAR(std::abs(dot(rotation, expected)), 1.0f, EPSILON);
    EXPECT_FALSE(HasUnevenChain(FlagsAt(0)));
}

TEST_F(WorldAccessorTest, UnevenScalePropagatesToDescendants) {
    auto root = Create(scaled(Vec3{1.0f, 2.0f, 1.0f}));
    Create(root, Transform::Identity());

    hierarchy.UpdateWorldTransforms();

    EXPECT_TRUE(HasUnevenChain(FlagsAt(0)));
    EXPECT_TRUE(HasUnevenChain(FlagsAt(1)));
}

TEST_F(WorldAccessorTest, EvenScaleLeavesTheChainClean) {
    auto root = Create(scaled(Vec3{2.0f, 2.0f, 2.0f}));
    Create(root, scaled(Vec3{0.5f, 0.5f, 0.5f}));

    hierarchy.UpdateWorldTransforms();

    EXPECT_FALSE(HasUnevenChain(FlagsAt(0)));
    EXPECT_FALSE(HasUnevenChain(FlagsAt(1)));
}

TEST_F(WorldAccessorTest, ChainClearsWhenTheParentTurnsEvenAgain) {
    auto root = Create(scaled(Vec3{1.0f, 2.0f, 1.0f}));
    auto child = Create(root, Transform::Identity());
    hierarchy.UpdateWorldTransforms();
    ASSERT_TRUE(HasUnevenChain(FlagsAt(1)));

    hierarchy.WriteLocalTransform(root).scale = Vec3{3.0f, 3.0f, 3.0f};
    hierarchy.UpdateWorldTransforms();

    EXPECT_FALSE(HasUnevenChain(FlagsAt(0)));
    EXPECT_FALSE(HasUnevenChain(FlagsAt(1)));
    EXPECT_TRUE(hierarchy.IsValid(child));
}

TEST_F(WorldAccessorTest, UpdateWarnsOnceAndOnlyBelowAnUnevenScale) {
    auto uneven = Create(scaled(Vec3{1.0f, 2.0f, 1.0f}));
    Create(uneven, Transform::Identity());
    Create(Transform::Identity());

    // nobody asked for a rotation, the scene itself is what is being reported;
    // the uneven node's own matrix is exact, so only its child is
    hierarchy.UpdateWorldTransforms();

    EXPECT_FALSE(HasWarned(FlagsAt(0)));
    EXPECT_TRUE(HasWarned(FlagsAt(1)));
    EXPECT_FALSE(HasWarned(FlagsAt(2)));

    // the bit is what keeps the next frame from logging again
    hierarchy.UpdateWorldTransforms();
    EXPECT_FALSE(HasWarned(FlagsAt(0)));
    EXPECT_TRUE(HasWarned(FlagsAt(1)));
    EXPECT_FALSE(HasWarned(FlagsAt(2)));
}

TEST_F(WorldAccessorTest, RotationSurvivesAZeroScale) {
    constexpr f32 QUARTER = std::numbers::pi_v<f32> / 2;

    auto local = rotationZ(QUARTER);
    local.scale = zeros();
    auto hidden = Create(local);
    auto child = Create(hidden, rotationZ(QUARTER));

    hierarchy.UpdateWorldTransforms();

    // reading the rotation back out of the basis would divide by zero here
    auto rotation = hierarchy.GetWorldRotation(child);
    auto expected = Vec4{0.0f, 0.0f, std::sin(QUARTER), std::cos(QUARTER)};
    EXPECT_NEAR(std::abs(dot(rotation, expected)), 1.0f, EPSILON);
}

TEST_F(WorldAccessorTest, RotationSurvivesAMirroringScale) {
    constexpr f32 QUARTER = std::numbers::pi_v<f32> / 2;

    auto local = rotationZ(QUARTER);
    local.scale = Vec3{-1.0f, 1.0f, 1.0f};
    auto mirrored = Create(local);

    hierarchy.UpdateWorldTransforms();

    // the mirror stays in the scale, it does not leak into the rotation
    auto rotation = hierarchy.GetWorldRotation(mirrored);
    auto expected =
        Vec4{0.0f, 0.0f, std::sin(QUARTER / 2), std::cos(QUARTER / 2)};
    EXPECT_NEAR(std::abs(dot(rotation, expected)), 1.0f, EPSILON);
}
