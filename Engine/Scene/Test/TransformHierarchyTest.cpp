#include <vector>
#include <gtest/gtest.h>
#include "Primitives.hpp"
#include "TransformHierarchy.hpp"

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
};

TEST_F(TransformHierarchyTest, EmptyHierarchy){
    EXPECT_TRUE(hierarchy.IsEmpty());
    EXPECT_FALSE(hierarchy.IsValid(TransformHandle::InvalidHandle()));
}

TEST_F(TransformHierarchyTest, CreateNodeDefaultsToIdentity){
    auto handle = hierarchy.CreateNode();

    EXPECT_TRUE(hierarchy.IsValid(handle));
    EXPECT_EQ(hierarchy.Size(), 1);
    expectSameTransform(hierarchy.GetLocalTransform(handle), Transform::Identity());
}

TEST_F(TransformHierarchyTest, CreateNodeKeepsGivenLocal){
    auto local = makeLocal(1.0f);
    auto handle = hierarchy.CreateNode(local);

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, SetLocalTransform){
    auto handle = hierarchy.CreateNode();
    auto local = makeLocal(2.0f);

    hierarchy.SetLocalTransform(handle, local);

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, SetLocalComponents){
    auto handle = hierarchy.CreateNode();
    auto local = makeLocal(3.0f);

    hierarchy.SetLocalPosition(handle, local.position);
    hierarchy.SetLocalRotation(handle, local.rotation);
    hierarchy.SetLocalScale(handle, local.scale);

    expectSameTransform(hierarchy.GetLocalTransform(handle), local);
}

TEST_F(TransformHierarchyTest, DestroyNodeInvalidatesOnlyItsHandle){
    auto first = hierarchy.CreateNode(makeLocal(1.0f));
    auto second = hierarchy.CreateNode(makeLocal(2.0f));
    auto third = hierarchy.CreateNode(makeLocal(3.0f));

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
    auto old = hierarchy.CreateNode(makeLocal(1.0f));
    hierarchy.DestroyNode(old);

    auto fresh = hierarchy.CreateNode(makeLocal(2.0f));

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

    EXPECT_EQ(hierarchy.Size(), 0);
}
