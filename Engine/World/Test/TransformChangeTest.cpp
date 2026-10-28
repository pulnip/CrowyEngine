#include <vector>

#include <gtest/gtest.h>

#include "TransformHierarchy.hpp"
#include "TransformHierarchyInspect.hpp"

using namespace Crowy;

namespace
{
    using Handles = std::vector<TransformHandle>;

    Handles updatedNodes(const TransformHierarchy& hierarchy) {
        const auto updated = hierarchy.GetUpdatedNodes();
        return Handles(updated.begin(), updated.end());
    }

    void commitAndUpdate(TransformHierarchy& hierarchy) {
        hierarchy.CommitStructuralChanges();
        hierarchy.UpdateWorldTransforms();

        Str error;
        ASSERT_TRUE(CheckInvariants(hierarchy, &error)) << error;
    }

    Transform at(Vec3 position) {
        return Transform{.position = position};
    }
}

TEST(TransformChange, ACommittedNodeIsUpdatedOnceAndThenLeftAlone) {
    TransformHierarchy hierarchy;
    const auto root = hierarchy.CreateNode(at({1.0f, 0.0f, 0.0f}));
    const auto child = hierarchy.CreateNode(root, at({0.0f, 1.0f, 0.0f}));

    commitAndUpdate(hierarchy);
    EXPECT_EQ(updatedNodes(hierarchy), (Handles{root, child}));

    hierarchy.UpdateWorldTransforms();
    EXPECT_TRUE(updatedNodes(hierarchy).empty());
    EXPECT_EQ(hierarchy.GetWorldPosition(child), (Vec3{1.0f, 1.0f, 0.0f}));
}

TEST(TransformChange, ALocalWriteUpdatesItsSubtreeAndNothingElse) {
    TransformHierarchy hierarchy;
    const auto moved = hierarchy.CreateNode();
    const auto child = hierarchy.CreateNode(moved, at({0.0f, 1.0f, 0.0f}));
    const auto other = hierarchy.CreateNode(at({5.0f, 0.0f, 0.0f}));
    commitAndUpdate(hierarchy);

    hierarchy.WriteLocalTransform(moved).position = {2.0f, 0.0f, 0.0f};
    hierarchy.UpdateWorldTransforms();

    EXPECT_EQ(updatedNodes(hierarchy), (Handles{moved, child}));
    EXPECT_EQ(hierarchy.GetWorldPosition(child), (Vec3{2.0f, 1.0f, 0.0f}));
    EXPECT_EQ(hierarchy.GetWorldPosition(other), (Vec3{5.0f, 0.0f, 0.0f}));
}

TEST(TransformChange, AWriteInPlaceUpdatesItsSubtree) {
    TransformHierarchy hierarchy;
    const auto moved = hierarchy.CreateNode();
    const auto child = hierarchy.CreateNode(moved, at({0.0f, 1.0f, 0.0f}));
    hierarchy.CreateNode(at({5.0f, 0.0f, 0.0f}));
    commitAndUpdate(hierarchy);

    hierarchy.WriteLocalTransform(moved).position = {2.0f, 0.0f, 0.0f};
    hierarchy.UpdateWorldTransforms();

    EXPECT_EQ(updatedNodes(hierarchy), (Handles{moved, child}));
    EXPECT_EQ(hierarchy.GetWorldPosition(child), (Vec3{2.0f, 1.0f, 0.0f}));
}

TEST(TransformChange, AWriteInPlaceBeforeTheCommitLandsAtIt) {
    TransformHierarchy hierarchy;
    const auto node = hierarchy.CreateNode(at({1.0f, 0.0f, 0.0f}));

    hierarchy.WriteLocalTransform(node).position = {3.0f, 0.0f, 0.0f};
    commitAndUpdate(hierarchy);

    EXPECT_EQ(updatedNodes(hierarchy), (Handles{node}));
    EXPECT_EQ(hierarchy.GetWorldPosition(node), (Vec3{3.0f, 0.0f, 0.0f}));
}

TEST(TransformChange, AMarkedNodeInsideAMarkedSubtreeIsUpdatedOnce) {
    TransformHierarchy hierarchy;
    const auto root = hierarchy.CreateNode();
    const auto child = hierarchy.CreateNode(root);
    commitAndUpdate(hierarchy);

    // marked child first, it still updates once, after its root
    hierarchy.WriteLocalTransform(child).scale = {2.0f, 2.0f, 2.0f};
    hierarchy.WriteLocalTransform(root).rotation = unitQuat();
    hierarchy.UpdateWorldTransforms();

    EXPECT_EQ(updatedNodes(hierarchy), (Handles{root, child}));
}

TEST(TransformChange, AReparentUpdatesTheMovedSubtree) {
    TransformHierarchy hierarchy;
    const auto parent = hierarchy.CreateNode(at({3.0f, 0.0f, 0.0f}));
    const auto moved = hierarchy.CreateNode(at({0.0f, 1.0f, 0.0f}));
    const auto movedChild = hierarchy.CreateNode(moved);
    commitAndUpdate(hierarchy);

    hierarchy.SetParent(moved, parent);
    commitAndUpdate(hierarchy);

    EXPECT_EQ(updatedNodes(hierarchy), (Handles{moved, movedChild}));
    EXPECT_EQ(hierarchy.GetWorldPosition(movedChild), (Vec3{3.0f, 1.0f, 0.0f}));
}

TEST(TransformChange, AMarkedNodeDestroyedBeforeTheUpdateIsSkipped) {
    TransformHierarchy hierarchy;
    const auto kept = hierarchy.CreateNode();
    const auto destroyed = hierarchy.CreateNode();
    commitAndUpdate(hierarchy);

    hierarchy.WriteLocalTransform(destroyed).position = {1.0f, 0.0f, 0.0f};
    hierarchy.DestroyNode(destroyed);
    hierarchy.CommitStructuralChanges();
    // the freed slot goes to a node still waiting for its commit
    const auto reissued = hierarchy.CreateNode();
    ASSERT_EQ(
        TransformNodeTable::SlotOf(reissued),
        TransformNodeTable::SlotOf(destroyed)
    );

    hierarchy.UpdateWorldTransforms();
    EXPECT_TRUE(updatedNodes(hierarchy).empty());
    EXPECT_TRUE(hierarchy.IsValid(kept));

    commitAndUpdate(hierarchy);
    EXPECT_EQ(updatedNodes(hierarchy), (Handles{reissued}));
}

TEST(TransformChange, AWriteToAPendingNodeLandsWithItsCommit) {
    TransformHierarchy hierarchy;
    const auto node = hierarchy.CreateNode();
    hierarchy.WriteLocalTransform(node).position = {0.0f, 0.0f, 4.0f};

    commitAndUpdate(hierarchy);
    EXPECT_EQ(updatedNodes(hierarchy), (Handles{node}));
    EXPECT_EQ(hierarchy.GetWorldPosition(node), (Vec3{0.0f, 0.0f, 4.0f}));
}
