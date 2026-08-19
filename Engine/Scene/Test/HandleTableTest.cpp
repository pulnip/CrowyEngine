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
    using Index = Table::Index;

    Index at(usize i){
        return Index{i};
    }
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
    auto handle = table.Acquire(at(7));

    EXPECT_TRUE(table.IsValid(handle));
    EXPECT_EQ(table.IndexOf(handle), at(7));
    EXPECT_EQ(table.SlotCount(), 1);
    EXPECT_EQ(table.LiveCount(), 1);
}

TEST_F(HandleTableTest, AcquireHandsOutDistinctSlots){
    auto first = table.Acquire(at(0));
    auto second = table.Acquire(at(1));

    EXPECT_NE(first.GetIndex(), second.GetIndex());
    EXPECT_EQ(table.IndexOf(first), at(0));
    EXPECT_EQ(table.IndexOf(second), at(1));
}

TEST_F(HandleTableTest, ReleaseInvalidatesOnlyItsHandle){
    auto first = table.Acquire(at(0));
    auto second = table.Acquire(at(1));

    table.Release(first);

    EXPECT_FALSE(table.IsValid(first));
    EXPECT_TRUE(table.IsValid(second));
    EXPECT_EQ(table.LiveCount(), 1);
    // a released slot stays counted, it is only parked for reuse
    EXPECT_EQ(table.SlotCount(), 2);
}

TEST_F(HandleTableTest, ReusedSlotGetsNewGeneration){
    auto old = table.Acquire(at(0));
    table.Release(old);

    auto fresh = table.Acquire(at(0));

    EXPECT_EQ(fresh.GetIndex(), old.GetIndex());
    EXPECT_NE(fresh.GetGeneration(), old.GetGeneration());
    EXPECT_FALSE(table.IsValid(old));
    EXPECT_TRUE(table.IsValid(fresh));
    EXPECT_EQ(table.SlotCount(), 1);
}

TEST_F(HandleTableTest, BindMovesLivingSlot){
    auto handle = table.Acquire(at(3));

    table.Bind(Table::SlotOf(handle), at(11));

    EXPECT_EQ(table.IndexOf(handle), at(11));
    EXPECT_EQ(table.IndexOf(Table::SlotOf(handle)), at(11));
}

TEST_F(HandleTableTest, HandleOfSlotRoundTrips){
    auto handle = table.Acquire(at(0));

    EXPECT_EQ(table.HandleOf(Table::SlotOf(handle)), handle);
}

TEST_F(HandleTableTest, HandleOfSlotAfterReuse){
    auto old = table.Acquire(at(0));
    table.Release(old);
    auto fresh = table.Acquire(at(0));

    auto recovered = table.HandleOf(Table::SlotOf(fresh));

    EXPECT_EQ(recovered, fresh);
    EXPECT_NE(recovered, old);
}

TEST_F(HandleTableTest, GrowthKeepsHandlesValid){
    constexpr usize COUNT = 1024;

    std::vector<Handle> handles;
    handles.reserve(COUNT);
    for(usize i=0; i<COUNT; ++i){
        handles.push_back(table.Acquire(at(i)));
    }

    EXPECT_EQ(table.SlotCount(), COUNT);
    EXPECT_EQ(table.LiveCount(), COUNT);
    for(usize i=0; i<COUNT; ++i){
        ASSERT_TRUE(table.IsValid(handles[i]));
        EXPECT_EQ(table.IndexOf(handles[i]), at(i));
    }
}

TEST_F(HandleTableTest, ReleaseAllThenReacquire){
    constexpr usize COUNT = 8;

    std::vector<Handle> handles;
    handles.reserve(COUNT);
    for(usize i=0; i<COUNT; ++i){
        handles.push_back(table.Acquire(at(i)));
    }
    for(auto handle: handles){
        table.Release(handle);
    }

    EXPECT_EQ(table.LiveCount(), 0);

    // every slot comes back from the free list, none is freshly appended
    for(usize i=0; i<COUNT; ++i){
        table.Acquire(at(i));
    }

    EXPECT_EQ(table.SlotCount(), COUNT);
    EXPECT_EQ(table.LiveCount(), COUNT);
    for(auto handle: handles){
        EXPECT_FALSE(table.IsValid(handle));
    }
}
