#include <cmath>
#include <numbers>
#include <gtest/gtest.h>
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "TransformHierarchyInspect.hpp"

using namespace Crowy;

namespace{
    constexpr f32 EPSILON = 1e-5f;

    Transform scaled(Vec3 scale){
        return Transform{.scale = scale};
    }

    Transform rotationZ(f32 radian){
        return Transform{
            .rotation = Vec4{0.0f, 0.0f, std::sin(radian/2), std::cos(radian/2)}
        };
    }

    void expectNearMatrix(const Mat4& lhs, const Mat4& rhs){
        for(usize column=0; column<4; ++column){
            for(usize row=0; row<4; ++row){
                EXPECT_NEAR(lhs[column][row], rhs[column][row], EPSILON)
                    << "at column " << column << ", row " << row;
            }
        }
    }
}

class WorldAccessorTest: public ::testing::Test{
protected:
    TransformHierarchy hierarchy;

    TransformHandle Create(const Transform& local){
        auto handle = hierarchy.CreateNode(local);
        hierarchy.CommitStructuralChanges();

        return handle;
    }
    TransformHandle Create(TransformHandle parent, const Transform& local){
        auto handle = hierarchy.CreateNode(parent, local);
        hierarchy.CommitStructuralChanges();

        return handle;
    }

    u32 FlagsAt(usize index) const{
        return MakeView(hierarchy).nodes[index].flags;
    }
    static bool HasUnevenChain(u32 flags){
        return (flags & TransformNode::NON_UNIFORM_IN_CHAIN) != 0;
    }
    static bool HasWarned(u32 flags){
        return (flags & TransformNode::WARNED_NON_UNIFORM) != 0;
    }
};

TEST_F(WorldAccessorTest, WorldPosition){
    auto root = Create(Transform{.position = Vec3{1.0f, 2.0f, 3.0f}});
    auto child = Create(root, Transform{.position = Vec3{0.0f, 1.0f, 0.0f}});

    hierarchy.UpdateWorldTransforms();

    auto position = hierarchy.GetWorldPosition(child);
    EXPECT_NEAR(position.x, 1.0f, EPSILON);
    EXPECT_NEAR(position.y, 3.0f, EPSILON);
    EXPECT_NEAR(position.z, 3.0f, EPSILON);
}

TEST_F(WorldAccessorTest, WorldRotationComposesTheChain){
    constexpr f32 QUARTER = std::numbers::pi_v<f32> / 2;

    auto root = Create(rotationZ(QUARTER));
    auto child = Create(root, rotationZ(QUARTER));

    hierarchy.UpdateWorldTransforms();

    // two quarter turns about +Z make a half turn
    auto rotation = hierarchy.GetWorldRotation(child);
    auto expected = Vec4{0.0f, 0.0f, std::sin(QUARTER), std::cos(QUARTER)};
    EXPECT_NEAR(std::abs(dot(rotation, expected)), 1.0f, EPSILON);
}

TEST_F(WorldAccessorTest, UniformScaleDoesNotDisturbTheRotation){
    constexpr f32 QUARTER = std::numbers::pi_v<f32> / 2;

    auto local = rotationZ(QUARTER);
    local.scale = Vec3{4.0f, 4.0f, 4.0f};
    auto root = Create(local);

    hierarchy.UpdateWorldTransforms();

    auto rotation = hierarchy.GetWorldRotation(root);
    auto expected = Vec4{0.0f, 0.0f, std::sin(QUARTER/2), std::cos(QUARTER/2)};
    EXPECT_NEAR(std::abs(dot(rotation, expected)), 1.0f, EPSILON);
    EXPECT_FALSE(HasUnevenChain(FlagsAt(0)));
}

TEST_F(WorldAccessorTest, UnevenScalePropagatesToDescendants){
    auto root = Create(scaled(Vec3{1.0f, 2.0f, 1.0f}));
    Create(root, Transform::Identity());

    hierarchy.UpdateWorldTransforms();

    EXPECT_TRUE(HasUnevenChain(FlagsAt(0)));
    EXPECT_TRUE(HasUnevenChain(FlagsAt(1)));
}

TEST_F(WorldAccessorTest, EvenScaleLeavesTheChainClean){
    auto root = Create(scaled(Vec3{2.0f, 2.0f, 2.0f}));
    Create(root, scaled(Vec3{0.5f, 0.5f, 0.5f}));

    hierarchy.UpdateWorldTransforms();

    EXPECT_FALSE(HasUnevenChain(FlagsAt(0)));
    EXPECT_FALSE(HasUnevenChain(FlagsAt(1)));
}

TEST_F(WorldAccessorTest, ChainClearsWhenTheParentTurnsEvenAgain){
    auto root = Create(scaled(Vec3{1.0f, 2.0f, 1.0f}));
    auto child = Create(root, Transform::Identity());
    hierarchy.UpdateWorldTransforms();
    ASSERT_TRUE(HasUnevenChain(FlagsAt(1)));

    hierarchy.SetLocalScale(root, Vec3{3.0f, 3.0f, 3.0f});
    hierarchy.UpdateWorldTransforms();

    EXPECT_FALSE(HasUnevenChain(FlagsAt(0)));
    EXPECT_FALSE(HasUnevenChain(FlagsAt(1)));
    EXPECT_TRUE(hierarchy.IsValid(child));
}

TEST_F(WorldAccessorTest, RotationWarnsOnceAndOnlyForTheUnevenNode){
    auto root = Create(scaled(Vec3{1.0f, 2.0f, 1.0f}));
    auto even = Create(Transform::Identity());
    hierarchy.UpdateWorldTransforms();

    ASSERT_FALSE(HasWarned(FlagsAt(0)));

    hierarchy.GetWorldRotation(root);
    EXPECT_TRUE(HasWarned(FlagsAt(0)));

    // the bit is what keeps a second call from logging again
    hierarchy.GetWorldRotation(root);
    EXPECT_TRUE(HasWarned(FlagsAt(0)));

    hierarchy.GetWorldRotation(even);
    EXPECT_FALSE(HasWarned(FlagsAt(1)));
}

TEST_F(WorldAccessorTest, InverseUndoesTheWorldMatrix){
    auto local = rotationZ(std::numbers::pi_v<f32> / 3);
    local.position = Vec3{2.0f, -1.0f, 4.0f};
    local.scale = Vec3{2.0f, 2.0f, 2.0f};

    auto root = Create(local);
    auto child = Create(root, Transform{.position = Vec3{1.0f, 1.0f, 1.0f}});
    hierarchy.UpdateWorldTransforms();

    expectNearMatrix(
        hierarchy.GetWorldMatrix(child) * hierarchy.GetWorldInverse(child),
        unitMat()
    );
}

TEST_F(WorldAccessorTest, InverseHandlesUnevenScale){
    auto root = Create(scaled(Vec3{2.0f, 4.0f, 0.5f}));
    hierarchy.UpdateWorldTransforms();

    expectNearMatrix(
        hierarchy.GetWorldMatrix(root) * hierarchy.GetWorldInverse(root),
        unitMat()
    );
}

TEST_F(WorldAccessorTest, InverseFollowsTheNextUpdate){
    auto root = Create(Transform{.position = Vec3{1.0f, 0.0f, 0.0f}});
    hierarchy.UpdateWorldTransforms();
    auto first = hierarchy.GetWorldInverse(root);

    hierarchy.SetLocalPosition(root, Vec3{5.0f, 0.0f, 0.0f});
    hierarchy.UpdateWorldTransforms();

    EXPECT_NEAR(first[3][0], -1.0f, EPSILON);
    EXPECT_NEAR(hierarchy.GetWorldInverse(root)[3][0], -5.0f, EPSILON);
}

TEST_F(WorldAccessorTest, InverseFollowsAStructuralChange){
    auto doomed = hierarchy.CreateNode(Transform{.position = Vec3{1.0f, 0.0f, 0.0f}});
    auto keeper = hierarchy.CreateNode(Transform{.position = Vec3{9.0f, 0.0f, 0.0f}});
    hierarchy.CommitStructuralChanges();
    hierarchy.UpdateWorldTransforms();

    // caches both, so a stale entry would answer for the wrong array position
    ASSERT_NEAR(hierarchy.GetWorldInverse(doomed)[3][0], -1.0f, EPSILON);
    ASSERT_NEAR(hierarchy.GetWorldInverse(keeper)[3][0], -9.0f, EPSILON);

    hierarchy.DestroyNode(doomed);
    hierarchy.CommitStructuralChanges();

    EXPECT_NEAR(hierarchy.GetWorldInverse(keeper)[3][0], -9.0f, EPSILON);
}
