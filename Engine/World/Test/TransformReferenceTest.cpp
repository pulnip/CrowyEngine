#include <cmath>
#include <numbers>
#include <vector>
#include <gtest/gtest.h>
#include "PairedHierarchy.hpp"
#include "Primitives.hpp"

using namespace Crowy;

namespace{
    Transform localOf(f32 seed){
        auto angle = seed * std::numbers::pi_v<f32> / 8;

        return Transform{
            .position = Vec3{seed, -seed, seed / 2},
            .rotation = Vec4{0.0f, 0.0f, std::sin(angle/2), std::cos(angle/2)},
            .scale = Vec3{1.0f, 1.0f, 1.0f}
        };
    }
}

class ReferenceComparisonTest: public ::testing::Test{
protected:
    PairedHierarchy paired;
};

TEST_F(ReferenceComparisonTest, EmptyHierarchy){
    paired.ExpectAgreement();

    EXPECT_EQ(paired.LivingCount(), 0);
}

TEST_F(ReferenceComparisonTest, SingleRoot){
    paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, ManyRoots){
    for(usize i=0; i<8; ++i){
        paired.Create(PairedHierarchy::NO_PARENT, localOf(static_cast<f32>(i)));
    }

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, MultiLevelTree){
    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    auto left = paired.Create(root, localOf(2.0f));
    auto right = paired.Create(root, localOf(3.0f));
    paired.Create(left, localOf(4.0f));
    paired.Create(left, localOf(5.0f));
    paired.Create(right, localOf(6.0f));

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, TreeGrowsAcrossSeveralCommits){
    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    paired.ExpectAgreement();

    auto branch = paired.Create(root, localOf(2.0f));
    paired.ExpectAgreement();

    paired.Create(branch, localOf(3.0f));
    paired.Create(root, localOf(4.0f));
    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, LocalWrittenAfterCommit){
    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    auto child = paired.Create(root, localOf(2.0f));
    paired.ExpectAgreement();

    paired.SetLocal(root, localOf(7.0f));
    paired.SetLocal(child, localOf(8.0f));

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, LocalWrittenBeforeTheFirstCommit){
    auto root = paired.Create(PairedHierarchy::NO_PARENT);
    auto child = paired.Create(root);

    paired.SetLocal(root, localOf(3.0f));
    paired.SetLocal(child, localOf(4.0f));

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, DestroyedLeafLeavesTheRestAlone){
    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    auto doomed = paired.Create(root, localOf(2.0f));
    auto keeper = paired.Create(root, localOf(3.0f));
    paired.Create(keeper, localOf(4.0f));
    paired.ExpectAgreement();

    paired.Destroy(doomed);

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, SubtreeDestroyedLeafFirst){
    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    auto branch = paired.Create(root, localOf(2.0f));
    auto leaf = paired.Create(branch, localOf(3.0f));
    auto keeper = paired.Create(root, localOf(4.0f));
    paired.ExpectAgreement();

    paired.Destroy(leaf);
    paired.Destroy(branch);

    paired.ExpectAgreement();
    EXPECT_EQ(paired.LivingCount(), 2);
    EXPECT_TRUE(paired.IsLeaf(keeper));
}

TEST_F(ReferenceComparisonTest, CreateAndDestroyShareOneBatch){
    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    auto doomed = paired.Create(root, localOf(2.0f));
    paired.ExpectAgreement();

    paired.Destroy(doomed);
    paired.Create(root, localOf(5.0f));
    paired.Create(root, localOf(6.0f));

    paired.ExpectAgreement();
}

TEST_F(ReferenceComparisonTest, WideFanout){
    constexpr usize WIDTH = 1000;

    auto root = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    for(usize i=0; i<WIDTH; ++i){
        paired.Create(root, localOf(static_cast<f32>(i % 8)));
    }

    paired.ExpectAgreement();
    EXPECT_EQ(paired.LivingCount(), WIDTH + 1);
}

TEST_F(ReferenceComparisonTest, DeepChain){
    constexpr usize DEPTH = 500;

    auto current = paired.Create(PairedHierarchy::NO_PARENT, localOf(1.0f));
    for(usize i=1; i<DEPTH; ++i){
        current = paired.Create(current, localOf(static_cast<f32>(i % 8)));
    }

    paired.ExpectAgreement();
    EXPECT_EQ(paired.LivingCount(), DEPTH);
}
