#include <utility>
#include <vector>
#include <gtest/gtest.h>
#include "TransformHierarchyInspect.hpp"

using namespace Crowy;

namespace{
    // A view of our own, so every invariant can be broken by hand
    struct Snapshot{
        std::vector<TransformNode> nodes;
        TransformNodeTable slots;

        HierarchyView View() const noexcept{
            return HierarchyView{
                .nodes = nodes,
                .slots = &slots
            };
        }

        // mirrors what a commit does, so a mutation can leave the rest consistent
        void RefreshParentIndexes() noexcept{
            for(auto& node: nodes){
                node.parentIndex = node.IsRoot() ?
                    TransformIndex::Invalid() :
                    slots.IndexOf(node.parentSlot);
            }
        }

        void SwapNodes(usize lhs, usize rhs) noexcept{
            std::swap(nodes[lhs], nodes[rhs]);
            slots.Bind(nodes[lhs].slot, TransformIndex{lhs});
            slots.Bind(nodes[rhs].slot, TransformIndex{rhs});
            RefreshParentIndexes();
        }
    };

    Snapshot snapshotOf(const TransformHierarchy& hierarchy){
        auto view = MakeView(hierarchy);

        return Snapshot{
            .nodes = {view.nodes.begin(), view.nodes.end()},
            .slots = *view.slots
        };
    }
}

// root -> {branch -> {leaf}, tail}, so preorder is root, branch, leaf, tail
class HierarchyViewTest: public ::testing::Test{
protected:
    TransformHierarchy hierarchy;
    TransformHandle root, branch, leaf, tail;
    Str error;

    void SetUp() override{
        root = hierarchy.CreateNode();
        branch = hierarchy.CreateNode(root);
        leaf = hierarchy.CreateNode(branch);
        tail = hierarchy.CreateNode(root);
        hierarchy.CommitStructuralChanges();
    }
};

TEST_F(HierarchyViewTest, IntactHierarchyPasses){
    ASSERT_TRUE(CheckInvariants(hierarchy, &error)) << error;
    EXPECT_TRUE(error.empty());
}

TEST_F(HierarchyViewTest, SnapshotOfAnIntactHierarchyPasses){
    auto snapshot = snapshotOf(hierarchy);

    EXPECT_TRUE(CheckInvariants(snapshot.View(), &error)) << error;
}

TEST_F(HierarchyViewTest, ViewWithoutSlotTableFails){
    auto snapshot = snapshotOf(hierarchy);
    auto view = snapshot.View();
    view.slots = nullptr;

    EXPECT_FALSE(CheckInvariants(view, &error));
    EXPECT_FALSE(error.empty());
}

TEST_F(HierarchyViewTest, EmptyViewPasses){
    HierarchyView view{};
    TransformNodeTable empty;
    view.slots = &empty;

    EXPECT_TRUE(CheckInvariants(view, &error)) << error;
}

TEST_F(HierarchyViewTest, ZeroSubtreeSizeFails){
    auto snapshot = snapshotOf(hierarchy);
    snapshot.nodes[2].subtreeSize = 0;

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("[2]"), Str::npos) << error;
}

// I3
TEST_F(HierarchyViewTest, SlotPointingAtAnotherNodeFails){
    auto snapshot = snapshotOf(hierarchy);
    snapshot.nodes[1].slot = snapshot.nodes[2].slot;

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("I3"), Str::npos) << error;
    EXPECT_NE(error.find("[1]"), Str::npos) << error;
}

TEST_F(HierarchyViewTest, ParentIndexDisagreeingWithParentSlotFails){
    auto snapshot = snapshotOf(hierarchy);
    // leaf's parent is branch at 1, not root at 0
    snapshot.nodes[2].parentIndex = TransformIndex{0};

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("I3"), Str::npos) << error;
    EXPECT_NE(error.find("[2]"), Str::npos) << error;
}

// I1
TEST_F(HierarchyViewTest, ParentSittingBehindItsChildFails){
    auto snapshot = snapshotOf(hierarchy);
    // leaf moves in front of branch, and the slot table follows, so only the
    // preorder itself is broken
    snapshot.SwapNodes(1, 2);

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("I1"), Str::npos) << error;
}

// I2
TEST_F(HierarchyViewTest, SubtreeReachingOverAStrangerFails){
    auto snapshot = snapshotOf(hierarchy);
    // branch claims tail, which belongs to root
    ++snapshot.nodes[1].subtreeSize;

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("I2"), Str::npos) << error;
    EXPECT_NE(error.find("[1]"), Str::npos) << error;
}

TEST_F(HierarchyViewTest, SubtreeRunningPastTheEndFails){
    auto snapshot = snapshotOf(hierarchy);
    // the root has no ancestor to catch it first, so the range check has to
    snapshot.nodes[0].subtreeSize = 10;

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("I2"), Str::npos) << error;
    EXPECT_NE(error.find("[0]"), Str::npos) << error;
}

// I4
TEST_F(HierarchyViewTest, RootLeavingANodeUncoveredFails){
    auto snapshot = snapshotOf(hierarchy);
    // every local check still holds, only the total coverage is short
    --snapshot.nodes[0].subtreeSize;

    EXPECT_FALSE(CheckInvariants(snapshot.View(), &error));
    EXPECT_NE(error.find("I4"), Str::npos) << error;
}
