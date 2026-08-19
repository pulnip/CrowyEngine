#include <cmath>
#include <numbers>
#include <vector>
#include <gtest/gtest.h>
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "TransformHierarchy.hpp"

using namespace Crowy;

namespace{
    // The tests keep their values around 1, and a world matrix here is a handful
    // of multiply-adds on f32, which holds about 7 decimal digits. 1e-5 leaves an
    // order of magnitude of room over the error that can actually accumulate.
    constexpr f32 EPSILON = 1e-5f;

    Transform translation(Vec3 position){
        return Transform{.position = position};
    }

    // right hand rule, around +Z
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

    Vec3 positionOf(const Mat4& world){
        return static_cast<Vec3>(world[3]);
    }
}

class TransformWorldTest: public ::testing::Test{
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
};

TEST_F(TransformWorldTest, UpdateOnEmptyHierarchy){
    hierarchy.UpdateWorldTransforms();

    EXPECT_TRUE(hierarchy.IsEmpty());
}

TEST_F(TransformWorldTest, RootWorldIsItsOwnLocal){
    auto local = translation(Vec3{1.0f, 2.0f, 3.0f});
    auto root = Create(local);

    hierarchy.UpdateWorldTransforms();

    expectNearMatrix(hierarchy.GetWorldMatrix(root), modelMat(local));
}

TEST_F(TransformWorldTest, ChildIsParentTimesLocal){
    auto parentLocal = translation(Vec3{1.0f, 0.0f, 0.0f});
    auto childLocal = translation(Vec3{0.0f, 2.0f, 0.0f});

    auto parent = Create(parentLocal);
    auto child = Create(parent, childLocal);

    hierarchy.UpdateWorldTransforms();

    expectNearMatrix(
        hierarchy.GetWorldMatrix(child),
        modelMat(parentLocal) * modelMat(childLocal)
    );
    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(child)).x, 1.0f, EPSILON);
    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(child)).y, 2.0f, EPSILON);
}

// catches a flipped multiplication order, which a translation-only case cannot
TEST_F(TransformWorldTest, ParentRotationCarriesTheChildAround){
    constexpr f32 QUARTER_TURN = std::numbers::pi_v<f32> / 2;

    auto parent = Create(rotationZ(QUARTER_TURN));
    auto child = Create(parent, translation(Vec3{1.0f, 0.0f, 0.0f}));

    hierarchy.UpdateWorldTransforms();

    // the child's +X offset comes out along +Y once the parent turned it
    auto position = positionOf(hierarchy.GetWorldMatrix(child));
    EXPECT_NEAR(position.x, 0.0f, EPSILON);
    EXPECT_NEAR(position.y, 1.0f, EPSILON);
    EXPECT_NEAR(position.z, 0.0f, EPSILON);
}

TEST_F(TransformWorldTest, SiblingsDoNotSeeEachOther){
    auto parent = Create(translation(Vec3{1.0f, 0.0f, 0.0f}));
    auto first = Create(parent, translation(Vec3{0.0f, 1.0f, 0.0f}));
    auto second = Create(parent, translation(Vec3{0.0f, 0.0f, 1.0f}));

    hierarchy.UpdateWorldTransforms();

    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(first)).y, 1.0f, EPSILON);
    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(first)).z, 0.0f, EPSILON);
    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(second)).y, 0.0f, EPSILON);
    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(second)).z, 1.0f, EPSILON);
}

TEST_F(TransformWorldTest, DeepChainNeedsNoRecursion){
    constexpr usize DEPTH = 1000;

    std::vector<TransformHandle> chain;
    chain.reserve(DEPTH);
    chain.push_back(hierarchy.CreateNode(translation(Vec3{1.0f, 0.0f, 0.0f})));
    for(usize i=1; i<DEPTH; ++i){
        chain.push_back(hierarchy.CreateNode(
            chain.back(),
            translation(Vec3{1.0f, 0.0f, 0.0f})
        ));
    }
    hierarchy.CommitStructuralChanges();

    hierarchy.UpdateWorldTransforms();

    // whole steps along one axis, so f32 carries them exactly
    for(usize i=0; i<DEPTH; ++i){
        ASSERT_EQ(
            positionOf(hierarchy.GetWorldMatrix(chain[i])).x,
            static_cast<f32>(i + 1)
        );
    }
    expectNearMatrix(
        hierarchy.ComputeWorldMatrixNow(chain.back()),
        hierarchy.GetWorldMatrix(chain.back())
    );
}

TEST_F(TransformWorldTest, ComputeNowAgreesWithTheCache){
    auto root = Create(translation(Vec3{1.0f, 0.0f, 0.0f}));
    auto branch = Create(root, rotationZ(std::numbers::pi_v<f32> / 3));
    auto leaf = Create(branch, translation(Vec3{0.0f, 2.0f, 1.0f}));

    hierarchy.UpdateWorldTransforms();

    expectNearMatrix(
        hierarchy.ComputeWorldMatrixNow(leaf),
        hierarchy.GetWorldMatrix(leaf)
    );
}

TEST_F(TransformWorldTest, ComputeNowLeavesTheCacheAlone){
    auto root = Create(translation(Vec3{1.0f, 0.0f, 0.0f}));
    auto child = Create(root, translation(Vec3{1.0f, 0.0f, 0.0f}));
    hierarchy.UpdateWorldTransforms();

    auto cached = hierarchy.GetWorldMatrix(child);
    hierarchy.SetLocalPosition(child, Vec3{5.0f, 0.0f, 0.0f});

    // the fresh answer moved, the cache did not
    EXPECT_NEAR(positionOf(hierarchy.ComputeWorldMatrixNow(child)).x, 6.0f, EPSILON);
    expectNearMatrix(hierarchy.GetWorldMatrix(child), cached);

    hierarchy.UpdateWorldTransforms();

    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(child)).x, 6.0f, EPSILON);
}

TEST_F(TransformWorldTest, WorldFollowsAStructuralChange){
    auto root = Create(translation(Vec3{1.0f, 0.0f, 0.0f}));
    auto first = Create(root, translation(Vec3{1.0f, 0.0f, 0.0f}));
    auto second = Create(root, translation(Vec3{2.0f, 0.0f, 0.0f}));
    hierarchy.UpdateWorldTransforms();

    // first moves out from under root, so second slides forward in the array
    hierarchy.DestroyNode(first);
    hierarchy.CommitStructuralChanges();
    hierarchy.UpdateWorldTransforms();

    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(second)).x, 3.0f, EPSILON);
    EXPECT_NEAR(positionOf(hierarchy.GetWorldMatrix(root)).x, 1.0f, EPSILON);
}
