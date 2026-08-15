#include <vector>
#include <gtest/gtest.h>
#include "Primitives.hpp"
#include "TransformHierarchyInspect.hpp"

using namespace Crowy;

namespace{
    Transform makeLocal(f32 seed){
        return Transform{
            .position = Vec3{seed, seed + 1.0f, seed + 2.0f},
            .rotation = Vec4{0.0f, 0.0f, 0.0f, 1.0f},
            .scale = Vec3{seed + 3.0f, seed + 4.0f, seed + 5.0f}
        };
    }

    void expectSameTransform(const Transform& lhs, const Transform& rhs){
        EXPECT_EQ(lhs.position, rhs.position);
        EXPECT_EQ(lhs.rotation, rhs.rotation);
        EXPECT_EQ(lhs.scale, rhs.scale);
    }
}

class TransformHierarchyTest: public ::testing::Test{
protected:
    TransformHierarchy hierarchy;

    void TearDown() override{
        Str error;
        EXPECT_TRUE(CheckInvariants(hierarchy, &error)) << error;
    }

    void Commit(){
        hierarchy.CommitStructuralChanges();

        Str error;
        ASSERT_TRUE(CheckInvariants(hierarchy, &error)) << error;
    }

    TransformHandle commitedNode(const Transform& local = Transform::Identity()){
        auto handle = hierarchy.CreateNode(local);
        Commit();

        return handle;
    }
};

TEST_F(TransformHierarchyTest, EmptyHierarchy){
    EXPECT_TRUE(hierarchy.IsEmpty());
    EXPECT_FALSE(hierarchy.HasPendingChanges());
    EXPECT_FALSE(hierarchy.IsValid(TransformHandle::InvalidHandle()));
}

TEST_F(TransformHierarchyTest, CommitOnEmptyHierarchy){
    Commit();

    EXPECT_TRUE(hierarchy.IsEmpty());
    EXPECT_FALSE(hierarchy.HasPendingChanges());
}

TEST_F(TransformHierarchyTest, CreateNodeDefaultsToIdentity){
    auto handle = commitedNode();

    EXPECT_TRUE(hierarchy.IsValid(handle));
    EXPECT_EQ(hierarchy.Size(), 1);
    expectSameTransform(hierarchy.GetLocalTransform(handle), Transform::Identity());
}

TEST_F(TransformHierarchyTest, CreateNodeKeepsGivenLocal){
    auto local = makeLocal(1.0f);
    auto handle = commitedNode(local);

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, SetLocalTransform){
    auto handle = commitedNode();
    auto local = makeLocal(2.0f);

    hierarchy.SetLocalTransform(handle, local);

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, SetLocalComponents){
    auto handle = commitedNode();
    auto local = makeLocal(3.0f);

    hierarchy.SetLocalPosition(handle, local.position);
    hierarchy.SetLocalRotation(handle, local.rotation);
    hierarchy.SetLocalScale(handle, local.scale);

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, CreationIsDeferredButHandleIsValid){
    auto handle = hierarchy.CreateNode(makeLocal(1.0f));

    EXPECT_TRUE(hierarchy.IsValid(handle));
    EXPECT_TRUE(hierarchy.HasPendingChanges());
    // the array has not moved yet
    EXPECT_TRUE(hierarchy.IsEmpty());

    Commit();

    EXPECT_TRUE(hierarchy.IsValid(handle));
    EXPECT_FALSE(hierarchy.HasPendingChanges());
    EXPECT_EQ(hierarchy.Size(), 1);
}

TEST_F(TransformHierarchyTest, LocalTransformWrittenBeforeCommitSurvivesIt){
    auto handle = hierarchy.CreateNode();
    auto local = makeLocal(4.0f);

    hierarchy.SetLocalTransform(handle, local);
    expectSameTransform(hierarchy.GetLocalTransform(handle), local);

    Commit();

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, ChildAttachesToParent){
    auto parent = commitedNode();
    auto child = hierarchy.CreateNode(parent);

    // structural queries answer as of the last commit
    EXPECT_FALSE(hierarchy.GetParent(child).IsValid());
    EXPECT_EQ(hierarchy.GetChildCount(parent), 0);

    Commit();

    EXPECT_EQ(hierarchy.Size(), 2);
    EXPECT_EQ(hierarchy.GetParent(child), parent);
    EXPECT_EQ(hierarchy.GetChildCount(parent), 1);
    EXPECT_FALSE(hierarchy.GetParent(parent).IsValid());
}

TEST_F(TransformHierarchyTest, ParentCreatedInTheSameCommit){
    auto parent = hierarchy.CreateNode();
    auto child = hierarchy.CreateNode(parent);

    Commit();

    EXPECT_EQ(hierarchy.Size(), 2);
    EXPECT_EQ(hierarchy.GetParent(child), parent);
    EXPECT_EQ(hierarchy.GetChildCount(parent), 1);
}

TEST_F(TransformHierarchyTest, SiblingsShareOneParent){
    auto parent = hierarchy.CreateNode();
    auto first = hierarchy.CreateNode(parent);
    auto second = hierarchy.CreateNode(parent);
    auto third = hierarchy.CreateNode(parent);

    Commit();

    EXPECT_EQ(hierarchy.GetChildCount(parent), 3);
    EXPECT_EQ(hierarchy.GetParent(first), parent);
    EXPECT_EQ(hierarchy.GetParent(second), parent);
    EXPECT_EQ(hierarchy.GetParent(third), parent);
}

TEST_F(TransformHierarchyTest, ChildLandsBehindAnExistingDeepSubtree){
    auto root = hierarchy.CreateNode();
    auto branch = hierarchy.CreateNode(root);
    auto leaf = hierarchy.CreateNode(branch);
    Commit();

    // root's subtree is 3 long now, so the newcomer has to clear all of it
    auto late = hierarchy.CreateNode(root);
    Commit();

    EXPECT_EQ(hierarchy.Size(), 4);
    EXPECT_EQ(hierarchy.GetChildCount(root), 2);
    EXPECT_EQ(hierarchy.GetChildCount(branch), 1);
    EXPECT_EQ(hierarchy.GetParent(leaf), branch);
    EXPECT_EQ(hierarchy.GetParent(late), root);
}

TEST_F(TransformHierarchyTest, DeepChainKeepsEveryLink){
    constexpr usize DEPTH = 1000;

    std::vector<TransformHandle> chain;
    chain.reserve(DEPTH);
    chain.push_back(hierarchy.CreateNode());
    for(usize i=1; i<DEPTH; ++i){
        chain.push_back(hierarchy.CreateNode(chain.back()));
    }
    Commit();

    EXPECT_EQ(hierarchy.Size(), DEPTH);
    for(usize i=1; i<DEPTH; ++i){
        ASSERT_EQ(hierarchy.GetParent(chain[i]), chain[i-1]);
    }
    EXPECT_EQ(hierarchy.GetChildCount(chain.back()), 0);
}

TEST_F(TransformHierarchyTest, CreateThenDestroyBeforeCommitCancelsOut){
    auto handle = hierarchy.CreateNode(makeLocal(1.0f));

    hierarchy.DestroyNode(handle);

    EXPECT_FALSE(hierarchy.IsValid(handle));
    EXPECT_FALSE(hierarchy.HasPendingChanges());

    Commit();

    EXPECT_TRUE(hierarchy.IsEmpty());
}

TEST_F(TransformHierarchyTest, DestroyingLeafShrinksItsAncestors){
    auto root = hierarchy.CreateNode();
    auto branch = hierarchy.CreateNode(root);
    auto leaf = hierarchy.CreateNode(branch);
    Commit();

    hierarchy.DestroyNode(leaf);

    EXPECT_EQ(hierarchy.Size(), 2);
    EXPECT_EQ(hierarchy.GetChildCount(branch), 0);
    EXPECT_EQ(hierarchy.GetChildCount(root), 1);
    EXPECT_EQ(hierarchy.GetParent(branch), root);

    // the shrunk chain still accepts a newcomer at the right place
    auto late = hierarchy.CreateNode(root);
    Commit();

    EXPECT_EQ(hierarchy.GetChildCount(root), 2);
    EXPECT_EQ(hierarchy.GetParent(late), root);
}

TEST_F(TransformHierarchyTest, DestroyNodeInvalidatesOnlyItsHandle){
    auto first = hierarchy.CreateNode(makeLocal(1.0f));
    auto second = hierarchy.CreateNode(makeLocal(2.0f));
    auto third = hierarchy.CreateNode(makeLocal(3.0f));
    Commit();

    hierarchy.DestroyNode(second);

    EXPECT_FALSE(hierarchy.IsValid(second));
    EXPECT_TRUE(hierarchy.IsValid(first));
    EXPECT_TRUE(hierarchy.IsValid(third));
    EXPECT_EQ(hierarchy.Size(), 2);

    // the survivor behind the hole moved, its handle must still find it
    expectSameTransform(hierarchy.GetLocalTransform(first), makeLocal(1.0f));
    expectSameTransform(hierarchy.GetLocalTransform(third), makeLocal(3.0f));
}

TEST_F(TransformHierarchyTest, DestroyedSlotIsReusedWithNewGeneration){
    auto old = commitedNode(makeLocal(1.0f));
    hierarchy.DestroyNode(old);

    auto fresh = commitedNode(makeLocal(2.0f));

    EXPECT_EQ(fresh.GetIndex(), old.GetIndex());
    EXPECT_NE(fresh.GetGeneration(), old.GetGeneration());
    EXPECT_FALSE(hierarchy.IsValid(old));
    EXPECT_TRUE(hierarchy.IsValid(fresh));
    expectSameTransform(hierarchy.GetLocalTransform(fresh), makeLocal(2.0f));
}

TEST_F(TransformHierarchyTest, SlotTableGrowthKeepsHandlesValid){
    constexpr usize COUNT = 1024;

    std::vector<TransformHandle> handles;
    handles.reserve(COUNT);
    for(usize i=0; i<COUNT; ++i){
        handles.push_back(hierarchy.CreateNode(makeLocal(static_cast<f32>(i))));
    }
    Commit();

    EXPECT_EQ(hierarchy.Size(), COUNT);
    for(usize i=0; i<COUNT; ++i){
        ASSERT_TRUE(hierarchy.IsValid(handles[i]));
        expectSameTransform(
            hierarchy.GetLocalTransform(handles[i]),
            makeLocal(static_cast<f32>(i))
        );
    }
}

TEST_F(TransformHierarchyTest, DestroyFromFrontKeepsRemainingLocals){
    constexpr usize COUNT = 16;

    std::vector<TransformHandle> handles;
    handles.reserve(COUNT);
    for(usize i=0; i<COUNT; ++i){
        handles.push_back(hierarchy.CreateNode(makeLocal(static_cast<f32>(i))));
    }
    Commit();

    for(usize i=0; i<COUNT; ++i){
        hierarchy.DestroyNode(handles[i]);

        EXPECT_EQ(hierarchy.Size(), COUNT - i - 1);
        for(usize j=i+1; j<COUNT; ++j){
            ASSERT_TRUE(hierarchy.IsValid(handles[j]));
            expectSameTransform(
                hierarchy.GetLocalTransform(handles[j]),
                makeLocal(static_cast<f32>(j))
            );
        }
    }

    EXPECT_TRUE(hierarchy.IsEmpty());
}
