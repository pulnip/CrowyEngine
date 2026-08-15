#include <vector>
#include <gtest/gtest.h>
#include "HandleTable.hpp"
#include "Primitives.hpp"

using namespace Crowy;

namespace
{
    struct Element{};

    using Table = HandleTable<Element>;
    using Handle = Table::Handle;
}

class HandleTableTest: public ::testing::Test{
protected:
    Table table;
};

TEST_F(HandleTableTest, EmptyTable){
    EXPECT_EQ(table.SlotCount(), 0);
    EXPECT_EQ(table.LiveCount(), 0);
    EXPECT_FALSE(table.IsValid(Handle::InvalidHandle()));
    EXPECT_FALSE(table.IsValid(Handle{}));
}

TEST_F(HandleTableTest, AcquireBindsIndex){
    auto handle = table.Acquire(7);

    EXPECT_TRUE(table.IsValid(handle));
    EXPECT_EQ(table.IndexOf(handle), 7);
    EXPECT_EQ(table.SlotCount(), 1);
    EXPECT_EQ(table.LiveCount(), 1);
}

TEST_F(HandleTableTest, AcquireHandsOutDistinctSlots){
    auto first = table.Acquire(0);
    auto second = table.Acquire(1);

    EXPECT_NE(first.GetIndex(), second.GetIndex());
    EXPECT_EQ(table.IndexOf(first), 0);
    EXPECT_EQ(table.IndexOf(second), 1);
}

TEST_F(HandleTableTest, ReleaseInvalidatesOnlyItsHandle){
    auto first = table.Acquire(0);
    auto second = table.Acquire(1);

    table.Release(first);

    EXPECT_FALSE(table.IsValid(first));
    EXPECT_TRUE(table.IsValid(second));
    EXPECT_EQ(table.LiveCount(), 1);
    // a released slot stays counted, it is only parked for reuse
    EXPECT_EQ(table.SlotCount(), 2);
}

TEST_F(HandleTableTest, ReusedSlotGetsNewGeneration){
    auto old = table.Acquire(0);
    table.Release(old);

    auto fresh = table.Acquire(0);

    EXPECT_EQ(fresh.GetIndex(), old.GetIndex());
    EXPECT_NE(fresh.GetGeneration(), old.GetGeneration());
    EXPECT_FALSE(table.IsValid(old));
    EXPECT_TRUE(table.IsValid(fresh));
    EXPECT_EQ(table.SlotCount(), 1);
}

TEST_F(HandleTableTest, BindMovesLivingSlot){
    auto handle = table.Acquire(3);

    table.Bind(handle.GetIndex(), 11);

    EXPECT_EQ(table.IndexOf(handle), 11);
    EXPECT_EQ(table.IndexOfSlot(handle.GetIndex()), 11);
}

TEST_F(HandleTableTest, HandleOfSlotRoundTrips){
    auto handle = table.Acquire(0);

    EXPECT_EQ(table.HandleOfSlot(handle.GetIndex()), handle);
}

TEST_F(HandleTableTest, HandleOfSlotAfterReuse){
    auto old = table.Acquire(0);
    table.Release(old);
    auto fresh = table.Acquire(0);

    auto recovered = table.HandleOfSlot(fresh.GetIndex());

    EXPECT_EQ(recovered, fresh);
    EXPECT_NE(recovered, old);
}

TEST_F(HandleTableTest, GrowthKeepsHandlesValid){
    constexpr usize COUNT = 1024;

    std::vector<Handle> handles;
    handles.reserve(COUNT);
    for(usize i=0; i<COUNT; ++i){
        handles.push_back(table.Acquire(i));
    }

    EXPECT_EQ(table.SlotCount(), COUNT);
    EXPECT_EQ(table.LiveCount(), COUNT);
    for(usize i=0; i<COUNT; ++i){
        ASSERT_TRUE(table.IsValid(handles[i]));
        EXPECT_EQ(table.IndexOf(handles[i]), i);
    }
}

TEST_F(HandleTableTest, ReleaseAllThenReacquire){
    constexpr usize COUNT = 8;

    std::vector<Handle> handles;
    handles.reserve(COUNT);
    for(usize i=0; i<COUNT; ++i){
        handles.push_back(table.Acquire(i));
    }
    for(auto handle: handles){
        table.Release(handle);
    }

    EXPECT_EQ(table.LiveCount(), 0);

    // every slot comes back from the free list, none is freshly appended
    for(usize i=0; i<COUNT; ++i){
        table.Acquire(i);
    }

    EXPECT_EQ(table.SlotCount(), COUNT);
    EXPECT_EQ(table.LiveCount(), COUNT);
    for(auto handle: handles){
        EXPECT_FALSE(table.IsValid(handle));
    }
}
