#include <vector>

#include <gtest/gtest.h>

#include "RenderScene.hpp"

using namespace Crowy;

namespace
{
    PrimitiveSnapshot At(f32 x) {
        return PrimitiveSnapshot{
            .localToWorld = translateMat({x, 0.0f, 0.0f}),
            .worldBounds = AABB3D{.center = {x, 0.0f, 0.0f}},
            .geometry = GeometryAllocation{.indexCount = 3}
        };
    }

    f32 XOf(const PrimitiveSnapshot& snapshot) {
        return snapshot.worldBounds.center.x;
    }
}

TEST(RenderScene, AddPacksInOrder) {
    RenderScene scene;

    scene.Add(At(0.0f));
    scene.Add(At(1.0f));
    scene.Add(At(2.0f));

    ASSERT_EQ(scene.PrimitiveCount(), 3u);
    EXPECT_EQ(XOf(scene.PrimitiveAt(0)), 0.0f);
    EXPECT_EQ(XOf(scene.PrimitiveAt(1)), 1.0f);
    EXPECT_EQ(XOf(scene.PrimitiveAt(2)), 2.0f);
}

TEST(RenderScene, WriteFindsTheRowThroughTheHandle) {
    RenderScene scene;

    scene.Add(At(0.0f));
    const auto handle = scene.Add(At(1.0f));
    scene.Add(At(2.0f));

    scene.Write(handle, At(9.0f));

    EXPECT_EQ(XOf(scene.Read(handle)), 9.0f);
    EXPECT_EQ(XOf(scene.PrimitiveAt(1)), 9.0f);
}

TEST(RenderScene, RemoveSwapsTheLastRowIntoTheHole) {
    RenderScene scene;

    const auto first = scene.Add(At(0.0f));
    const auto middle = scene.Add(At(1.0f));
    const auto last = scene.Add(At(2.0f));

    scene.Remove(middle);

    ASSERT_EQ(scene.PrimitiveCount(), 2u);
    EXPECT_FALSE(scene.IsValid(middle));

    // its handle followed it into the hole
    EXPECT_TRUE(scene.IsValid(last));
    EXPECT_EQ(XOf(scene.Read(last)), 2.0f);
    EXPECT_EQ(XOf(scene.PrimitiveAt(1)), 2.0f);

    EXPECT_TRUE(scene.IsValid(first));
    EXPECT_EQ(XOf(scene.Read(first)), 0.0f);
}

TEST(RenderScene, RemoveLastNeedsNoSwap) {
    RenderScene scene;

    const auto first = scene.Add(At(0.0f));
    const auto last = scene.Add(At(1.0f));

    scene.Remove(last);

    ASSERT_EQ(scene.PrimitiveCount(), 1u);
    EXPECT_FALSE(scene.IsValid(last));
    EXPECT_TRUE(scene.IsValid(first));
    EXPECT_EQ(XOf(scene.Read(first)), 0.0f);
}

// the generation bump is all that separates the two handles
// 두 핸들을 갈라놓는 건 generation 증가뿐이다
TEST(RenderScene, ReusedSlotDoesNotReviveTheOldHandle) {
    RenderScene scene;

    const auto stale = scene.Add(At(0.0f));
    scene.Remove(stale);
    const auto fresh = scene.Add(At(1.0f));

    EXPECT_FALSE(scene.IsValid(stale));
    EXPECT_TRUE(scene.IsValid(fresh));
    EXPECT_EQ(XOf(scene.Read(fresh)), 1.0f);
}

TEST(RenderScene, RemoveEveryRowInOrder) {
    RenderScene scene;

    std::vector<PrimitiveHandle> handles;
    for(u32 i = 0; i < 8; ++i) {
        handles.push_back(scene.Add(At(static_cast<f32>(i))));
    }

    for(usize i = 0; i < handles.size(); ++i) {
        for(usize j = i; j < handles.size(); ++j) {
            ASSERT_TRUE(scene.IsValid(handles[j]));
            EXPECT_EQ(XOf(scene.Read(handles[j])), static_cast<f32>(j));
        }
        scene.Remove(handles[i]);
    }

    EXPECT_EQ(scene.PrimitiveCount(), 0u);
}

TEST(RenderScene, ClearExpiresEveryHandle) {
    RenderScene scene;

    const auto handle = scene.Add(At(0.0f));
    scene.Clear();

    EXPECT_EQ(scene.PrimitiveCount(), 0u);
    EXPECT_FALSE(scene.IsValid(handle));
}
