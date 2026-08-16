#include <vector>
#include <gtest/gtest.h>
#include "PairedHierarchy.hpp"
#include "Primitives.hpp"
#include "TransformHierarchyInspect.hpp"

using namespace Crowy;

namespace{
    Transform translation(f32 x){
        return Transform{.position = Vec3{x, 0.0f, 0.0f}};
    }

    constexpr auto NO_PARENT = PairedHierarchy::NO_PARENT;
}

class ReparentTest: public ::testing::Test{
protected:
    TransformHierarchy hierarchy;

    void Commit(){
        hierarchy.CommitStructuralChanges();

        Str error;
        ASSERT_TRUE(CheckInvariants(hierarchy, &error)) << error;
    }

    std::vector<TransformSlot> Order() const{
        std::vector<TransformSlot> order;
        for(const auto& node: MakeView(hierarchy).nodes){
            order.push_back(node.slot);
        }

        return order;
    }
    static TransformSlot SlotOf(TransformHandle handle){
        return TransformNodeTable::SlotOf(handle);
    }
};

TEST_F(ReparentTest, MoveToALaterSubtree){
    // first, second are roots; the block travels forward in the array
    auto first = hierarchy.CreateNode();
    auto moving = hierarchy.CreateNode(first);
    auto second = hierarchy.CreateNode();
    Commit();

    hierarchy.SetParent(moving, second);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(moving), second);
    EXPECT_EQ(hierarchy.GetChildCount(first), 0);
    EXPECT_EQ(hierarchy.GetChildCount(second), 1);
    EXPECT_EQ(Order(), (std::vector{SlotOf(first), SlotOf(second), SlotOf(moving)}));
}

TEST_F(ReparentTest, MoveToAnEarlierSubtree){
    // the mirror image: the block travels backward in the array
    auto first = hierarchy.CreateNode();
    auto second = hierarchy.CreateNode();
    auto moving = hierarchy.CreateNode(second);
    Commit();

    hierarchy.SetParent(moving, first);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(moving), first);
    EXPECT_EQ(hierarchy.GetChildCount(first), 1);
    EXPECT_EQ(hierarchy.GetChildCount(second), 0);
    EXPECT_EQ(Order(), (std::vector{SlotOf(first), SlotOf(moving), SlotOf(second)}));
}

TEST_F(ReparentTest, WholeSubtreeTravelsWithItsRoot){
    auto first = hierarchy.CreateNode();
    auto moving = hierarchy.CreateNode(first);
    auto child = hierarchy.CreateNode(moving);
    auto grandchild = hierarchy.CreateNode(child);
    auto second = hierarchy.CreateNode();
    Commit();

    hierarchy.SetParent(moving, second);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(moving), second);
    EXPECT_EQ(hierarchy.GetParent(child), moving);
    EXPECT_EQ(hierarchy.GetParent(grandchild), child);
    EXPECT_EQ(hierarchy.GetChildCount(first), 0);
    EXPECT_EQ(hierarchy.GetChildCount(second), 1);
    EXPECT_EQ(Order(), (std::vector{
        SlotOf(first), SlotOf(second),
        SlotOf(moving), SlotOf(child), SlotOf(grandchild)
    }));
}

TEST_F(ReparentTest, PromoteToRoot){
    auto root = hierarchy.CreateNode();
    auto branch = hierarchy.CreateNode(root);
    auto leaf = hierarchy.CreateNode(branch);
    Commit();

    hierarchy.SetParent(branch, TransformHandle::InvalidHandle());
    Commit();

    EXPECT_FALSE(hierarchy.GetParent(branch).IsValid());
    EXPECT_EQ(hierarchy.GetParent(leaf), branch);
    EXPECT_EQ(hierarchy.GetChildCount(root), 0);
    EXPECT_EQ(hierarchy.GetChildCount(branch), 1);
}

TEST_F(ReparentTest, RootBecomesAChild){
    auto host = hierarchy.CreateNode();
    auto guest = hierarchy.CreateNode();
    auto guestChild = hierarchy.CreateNode(guest);
    Commit();

    hierarchy.SetParent(guest, host);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(guest), host);
    EXPECT_EQ(hierarchy.GetParent(guestChild), guest);
    EXPECT_EQ(hierarchy.GetChildCount(host), 1);
}

TEST_F(ReparentTest, MoveUpToTheGrandparent){
    auto root = hierarchy.CreateNode();
    auto branch = hierarchy.CreateNode(root);
    auto leaf = hierarchy.CreateNode(branch);
    auto tail = hierarchy.CreateNode(root);
    Commit();

    hierarchy.SetParent(leaf, root);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(leaf), root);
    EXPECT_EQ(hierarchy.GetChildCount(root), 3);
    EXPECT_EQ(hierarchy.GetChildCount(branch), 0);
    // the newcomer clears the whole existing subtree of its new parent
    EXPECT_EQ(Order(), (std::vector{
        SlotOf(root), SlotOf(branch), SlotOf(tail), SlotOf(leaf)
    }));
}

TEST_F(ReparentTest, ReparentingToTheSameParentChangesNothing){
    auto root = hierarchy.CreateNode();
    auto first = hierarchy.CreateNode(root);
    auto second = hierarchy.CreateNode(root);
    Commit();
    auto before = Order();

    hierarchy.SetParent(first, root);
    hierarchy.SetParent(root, TransformHandle::InvalidHandle());
    Commit();

    EXPECT_EQ(Order(), before);
    EXPECT_EQ(hierarchy.GetChildCount(root), 2);
    EXPECT_EQ(hierarchy.GetParent(second), root);
}

TEST_F(ReparentTest, SeveralMovesInOneCommit){
    auto first = hierarchy.CreateNode();
    auto second = hierarchy.CreateNode();
    auto third = hierarchy.CreateNode();
    auto moving = hierarchy.CreateNode(first);
    Commit();

    // each command has to look its node up again, the one before it moved things
    hierarchy.SetParent(moving, second);
    hierarchy.SetParent(moving, third);
    hierarchy.SetParent(second, third);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(moving), third);
    EXPECT_EQ(hierarchy.GetParent(second), third);
    EXPECT_EQ(hierarchy.GetChildCount(third), 2);
    EXPECT_EQ(hierarchy.GetChildCount(first), 0);
}

TEST_F(ReparentTest, ParentBornInTheSameCommit){
    auto root = hierarchy.CreateNode();
    auto moving = hierarchy.CreateNode(root);
    Commit();

    auto fresh = hierarchy.CreateNode();
    hierarchy.SetParent(moving, fresh);
    Commit();

    EXPECT_EQ(hierarchy.GetParent(moving), fresh);
    EXPECT_EQ(hierarchy.GetChildCount(fresh), 1);
    EXPECT_EQ(hierarchy.GetChildCount(root), 0);
}

TEST_F(ReparentTest, ReparentingAnUncommittedNodeRewritesItsPlan){
    auto first = hierarchy.CreateNode();
    auto second = hierarchy.CreateNode();
    Commit();

    auto fresh = hierarchy.CreateNode(first);
    hierarchy.SetParent(fresh, second);
    Commit();

    EXPECT_EQ(hierarchy.Size(), 3);
    EXPECT_EQ(hierarchy.GetParent(fresh), second);
    EXPECT_EQ(hierarchy.GetChildCount(first), 0);
    EXPECT_EQ(hierarchy.GetChildCount(second), 1);
}

class ReparentAgainstReferenceTest: public ::testing::Test{
protected:
    PairedHierarchy paired;
};

TEST_F(ReparentAgainstReferenceTest, ForwardAndBackwardAgree){
    auto first = paired.Create(NO_PARENT, translation(1.0f));
    auto firstChild = paired.Create(first, translation(2.0f));
    auto second = paired.Create(NO_PARENT, translation(10.0f));
    paired.Create(second, translation(20.0f));
    paired.ExpectAgreement();

    paired.SetParent(firstChild, second);
    paired.ExpectAgreement();

    paired.SetParent(firstChild, first);
    paired.ExpectAgreement();
}

TEST_F(ReparentAgainstReferenceTest, SubtreeMoveAgrees){
    auto root = paired.Create(NO_PARENT, translation(1.0f));
    auto branch = paired.Create(root, translation(2.0f));
    auto leaf = paired.Create(branch, translation(4.0f));
    paired.Create(leaf, translation(8.0f));
    auto host = paired.Create(NO_PARENT, translation(100.0f));
    paired.ExpectAgreement();

    paired.SetParent(branch, host);
    paired.ExpectAgreement();

    paired.SetParent(branch, NO_PARENT);
    paired.ExpectAgreement();
}

TEST_F(ReparentAgainstReferenceTest, MovesMixedWithCreatesAndDestroys){
    auto first = paired.Create(NO_PARENT, translation(1.0f));
    auto second = paired.Create(NO_PARENT, translation(10.0f));
    auto moving = paired.Create(first, translation(2.0f));
    auto doomed = paired.Create(first, translation(3.0f));
    paired.ExpectAgreement();

    paired.Destroy(doomed);
    auto fresh = paired.Create(second, translation(30.0f));
    paired.SetParent(moving, fresh);
    paired.ExpectAgreement();

    paired.SetParent(fresh, NO_PARENT);
    paired.ExpectAgreement();
}
