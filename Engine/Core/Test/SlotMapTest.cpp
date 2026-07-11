#include <vector>
#include <gtest/gtest.h>
#include "Primitives.hpp"
#include "Semantics.hpp"
#include "SlotMap.hpp"

using namespace Crowy;

struct LifetimeTracker{
    static u32 constructed;
    static u32 destructed;
    static u32 moved;

    static void reset(){
        constructed = destructed = moved = 0;
    }

    u32 value;

    LifetimeTracker(u32 v = 0)
        :value(v)
    {
        ++constructed;
    }
    ~LifetimeTracker(){
        ++destructed;
    }
    LifetimeTracker(LifetimeTracker&& other) noexcept
        :value(other.value)
    {
        ++moved;
    }
    LifetimeTracker& operator=(LifetimeTracker&& other) noexcept{
        value = other.value;
        ++moved;
        return *this;
    }

    SMOL_DECLARE_NON_COPYABLE(LifetimeTracker)
};

u32 LifetimeTracker::constructed = 0;
u32 LifetimeTracker::destructed = 0;
u32 LifetimeTracker::moved = 0;

class SlotMapTest: public ::testing::Test{
protected:
    SlotMap<i32> intMap;
    SlotMap<Str> stringMap;

    void SetUp() override{
        LifetimeTracker::reset();
    }
};

TEST_F(SlotMapTest, DefaultConstruction){
    SlotMap<i32> map;
    EXPECT_EQ(map.Size(), 0);
    EXPECT_EQ(map.Capacity(), 0);
}

TEST_F(SlotMapTest, PushAndFind){
    auto handle = intMap.Push(42);

    EXPECT_EQ(intMap.Size(), 1);

    auto ptr = intMap.Find(handle);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(*ptr, 42);
}

TEST_F(SlotMapTest, PushAndGet){
    auto handle = intMap.Push(42);

    EXPECT_EQ(intMap.GetRef(handle), 42);
}

TEST_F(SlotMapTest, EmplaceString){
    auto handle = stringMap.Emplace("hello world");

    EXPECT_EQ(stringMap.Size(), 1);
    EXPECT_EQ(stringMap.GetRef(handle), "hello world");
}

TEST_F(SlotMapTest, MultiplePush){
    auto h1 = intMap.Push(1);
    auto h2 = intMap.Push(2);
    auto h3 = intMap.Push(3);

    EXPECT_EQ(intMap.Size(), 3);
    EXPECT_EQ(intMap.GetRef(h1), 1);
    EXPECT_EQ(intMap.GetRef(h2), 2);
    EXPECT_EQ(intMap.GetRef(h3), 3);
}

TEST_F(SlotMapTest, RemoveBasic){
    auto handle = intMap.Push(42);
    EXPECT_TRUE(intMap.Remove(handle));
    EXPECT_EQ(intMap.Size(), 0);
}

TEST_F(SlotMapTest, RemoveInvalidatesHandle){
    auto handle = intMap.Push(42);
    intMap.Remove(handle);

    EXPECT_EQ(intMap.Find(handle), nullptr);
}

TEST_F(SlotMapTest, RemoveAndReuse){
    auto h1 = intMap.Push(1);
    auto h2 = intMap.Push(2);

    intMap.Remove(h1);

    auto h3 = intMap.Push(3);

    EXPECT_EQ(h3.GetIndex(), h1.GetIndex());
    EXPECT_NE(h3.GetGeneration(), h1.GetGeneration());

    EXPECT_EQ(intMap.GetRef(h3), 3);
    EXPECT_EQ(intMap.GetRef(h2), 2);

    EXPECT_EQ(intMap.Find(h1), nullptr);
}

TEST_F(SlotMapTest, RemoveExpiredHandleReturnsFalse){
    auto h1 = intMap.Push(1);
    intMap.Remove(h1);

    EXPECT_FALSE(intMap.Remove(h1));
}

TEST_F(SlotMapTest, RemoveOutOfRangeReturnsFalse){
    GenericHandle<int> invalidHandle(999, 0);
    EXPECT_FALSE(intMap.Remove(invalidHandle));
}

TEST_F(SlotMapTest, Clear){
    intMap.Push(1);
    intMap.Push(2);
    intMap.Push(3);

    intMap.Clear();

    EXPECT_EQ(intMap.Size(), 0);
    EXPECT_EQ(intMap.Capacity(), 3);
}

TEST_F(SlotMapTest, ClearInvalidatesAllHandles){
    auto h1 = intMap.Push(1);
    auto h2 = intMap.Push(2);

    intMap.Clear();

    EXPECT_EQ(intMap.Find(h1), nullptr);
    EXPECT_EQ(intMap.Find(h2), nullptr);
}

TEST_F(SlotMapTest, PushAfterClear){
    auto h1 = intMap.Push(1);
    intMap.Clear();

    auto h2 = intMap.Push(2);

    EXPECT_EQ(intMap.Size(), 1);
    EXPECT_EQ(intMap.GetRef(h2), 2);
    EXPECT_EQ(intMap.Find(h1), nullptr);
}

TEST_F(SlotMapTest, Reserve){
    intMap.Reserve(100);

    EXPECT_EQ(intMap.Size(), 0);
    EXPECT_GE(intMap.Capacity(), 100);
}

TEST_F(SlotMapTest, ReserveDoesNotShrink){
    intMap.Reserve(100);
    intMap.Reserve(50);

    EXPECT_GE(intMap.Capacity(), 100);
}

TEST_F(SlotMapTest, IteratorEmpty){
    EXPECT_EQ(intMap.begin(), intMap.end());
}

TEST_F(SlotMapTest, IteratorBasic){
    intMap.Push(1);
    intMap.Push(2);
    intMap.Push(3);

    std::vector<int> values;
    for(const auto& v: intMap){
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 3);

    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 2);
    EXPECT_EQ(values[2], 3);
}

TEST_F(SlotMapTest, IteratorSkipsFreeSlots){
    auto h1 = intMap.Push(1);
    intMap.Push(2);
    intMap.Push(3);

    intMap.Remove(h1);

    std::vector<int> values;
    for(const auto& v: intMap){
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 2);
    EXPECT_EQ(values[0], 2);
    EXPECT_EQ(values[1], 3);
}

TEST_F(SlotMapTest, IteratorWithHoles){
    auto h1 = intMap.Push(1);
    auto h2 = intMap.Push(2);
    auto h3 = intMap.Push(3);
    auto h4 = intMap.Push(4);
    auto h5 = intMap.Push(5);

    intMap.Remove(h2);
    intMap.Remove(h4);

    std::vector<int> values;
    for(const auto& v: intMap){
        values.push_back(v);
    }

    EXPECT_EQ(values.size(), 3);
    EXPECT_EQ(values[0], 1);
    EXPECT_EQ(values[1], 3);
    EXPECT_EQ(values[2], 5);
}

TEST_F(SlotMapTest, ConstIterator){
    intMap.Push(1);
    intMap.Push(2);

    i32 sum = 0;
    for(const auto& i: intMap){
        sum += i;
    }

    EXPECT_EQ(sum, 3);
}

TEST_F(SlotMapTest, IteratorModification){
    intMap.Push(1);
    intMap.Push(2);

    for(auto& v: intMap){
        v *= 10;
    }

    std::vector<i32> values;
    for(const auto& v: intMap){
        values.push_back(v);
    }

    EXPECT_EQ(values[0], 10);
    EXPECT_EQ(values[1], 20);
}

TEST(SlotMapLifetimeTest, DestructorCallsElementDestructors){
    LifetimeTracker::reset();

    {
        SlotMap<LifetimeTracker> map;
        map.Reserve(3);

        map.Emplace(1);
        map.Emplace(2);
        map.Emplace(3);
    }

    EXPECT_EQ(LifetimeTracker::destructed, 3);
}

TEST(SlotMapLifetimeTest, RemoveCallsDestructor){
    LifetimeTracker::reset();

    SlotMap<LifetimeTracker> map;
    auto h = map.Emplace(42);

    auto destructedBefore = LifetimeTracker::destructed;
    map.Remove(h);

    EXPECT_GT(LifetimeTracker::destructed, destructedBefore);
}

TEST(SlotMapLifetimeTest, ClearCallsDestructors){
    LifetimeTracker::reset();

    SlotMap<LifetimeTracker> map;
    map.Emplace(1);
    map.Emplace(2);
    map.Emplace(3);

    auto destructedBefore = LifetimeTracker::destructed;
    map.Clear();

    EXPECT_EQ(LifetimeTracker::destructed - destructedBefore, 3);
}

TEST_F(SlotMapTest, ManyInsertRemoveCycles){
    std::vector<GenericHandle<i32>> handles;

    for(i32 cycle=0; cycle<10; ++cycle){
        handles.clear();

        for(i32 i=0; i<100; ++i){
            handles.push_back(intMap.Push(i));
        }

        for(i32 i=0; i<100; i+=2){
            intMap.Remove(handles[i]);
        }

        EXPECT_EQ(intMap.Size(), 50);

        for(i32 i=1; i<100; i+=2){
            EXPECT_EQ(intMap.GetRef(handles[i]), i);
        }

        intMap.Clear();
    }
}

TEST_F(SlotMapTest, HandleStaysValidAfterOtherOperations){
    auto h1 = intMap.Push(1);

    auto h2 = intMap.Push(2);
    auto h3 = intMap.Push(3);
    intMap.Remove(h2);
    auto h4 = intMap.Push(4);

    EXPECT_NE(intMap.Find(h1), nullptr);
    EXPECT_EQ(intMap.GetRef(h1), 1);
}

TEST_F(SlotMapTest, GenerationOverflow){
    auto handle = intMap.Push(1);

    for(i32 i=0; i<1000; ++i){
        intMap.Remove(handle);
        handle = intMap.Push(i);
    }

    EXPECT_EQ(intMap.GetRef(handle), 999);
}

TEST(SlotMapMoveOnlyTest, UniquePtr){
    SlotMap<RAII<i32>> map;

    auto h = map.Push(std::make_unique<i32>(42));

    EXPECT_EQ(**map.Find(h), 42);

    map.Remove(h);
    EXPECT_EQ(map.Find(h), nullptr);
}
