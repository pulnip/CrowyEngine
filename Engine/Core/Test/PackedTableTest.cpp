#include <vector>

#include <gtest/gtest.h>

#include "PackedTable.hpp"

using namespace Crowy;

namespace
{
    struct Row {
        int value = 0;
    };

    using Table = PackedTable<Row>;
}

TEST(PackedTable, AddPacksInOrder) {
    Table table;

    table.Add(Row{0});
    table.Add(Row{1});
    table.Add(Row{2});

    ASSERT_EQ(table.Count(), 3u);
    EXPECT_EQ(table.At(0).value, 0);
    EXPECT_EQ(table.At(1).value, 1);
    EXPECT_EQ(table.At(2).value, 2);
}

TEST(PackedTable, WriteFindsTheRowThroughTheHandle) {
    Table table;

    table.Add(Row{0});
    const auto handle = table.Add(Row{1});
    table.Add(Row{2});

    table.Write(handle, Row{9});

    EXPECT_EQ(table.Read(handle).value, 9);
    EXPECT_EQ(table.At(1).value, 9);
}

TEST(PackedTable, RemoveSwapsTheLastRowIntoTheHole) {
    Table table;

    const auto first = table.Add(Row{0});
    const auto middle = table.Add(Row{1});
    const auto last = table.Add(Row{2});

    table.Remove(middle);

    ASSERT_EQ(table.Count(), 2u);
    EXPECT_FALSE(table.IsValid(middle));

    // its handle followed it into the hole
    EXPECT_TRUE(table.IsValid(last));
    EXPECT_EQ(table.Read(last).value, 2);
    EXPECT_EQ(table.At(1).value, 2);

    EXPECT_TRUE(table.IsValid(first));
    EXPECT_EQ(table.Read(first).value, 0);
}

// IndexOf is what a GPU row index is read from, so it has to track the swap
TEST(PackedTable, IndexOfFollowsTheSwap) {
    Table table;

    table.Add(Row{0});
    const auto middle = table.Add(Row{1});
    const auto last = table.Add(Row{2});

    EXPECT_EQ(table.IndexOf(last), 2u);
    table.Remove(middle);
    EXPECT_EQ(table.IndexOf(last), 1u);
}

TEST(PackedTable, RemoveLastNeedsNoSwap) {
    Table table;

    const auto first = table.Add(Row{0});
    const auto last = table.Add(Row{1});

    table.Remove(last);

    ASSERT_EQ(table.Count(), 1u);
    EXPECT_FALSE(table.IsValid(last));
    EXPECT_TRUE(table.IsValid(first));
    EXPECT_EQ(table.Read(first).value, 0);
}

// the generation bump is all that separates the two handles
TEST(PackedTable, ReusedSlotDoesNotReviveTheOldHandle) {
    Table table;

    const auto stale = table.Add(Row{0});
    table.Remove(stale);
    const auto fresh = table.Add(Row{1});

    EXPECT_FALSE(table.IsValid(stale));
    EXPECT_TRUE(table.IsValid(fresh));
    EXPECT_EQ(table.Read(fresh).value, 1);
}

TEST(PackedTable, RemoveEveryRowInOrder) {
    Table table;

    std::vector<Table::Handle> handles;
    for(int i = 0; i < 8; ++i) {
        handles.push_back(table.Add(Row{i}));
    }

    for(usize i = 0; i < handles.size(); ++i) {
        for(usize j = i; j < handles.size(); ++j) {
            ASSERT_TRUE(table.IsValid(handles[j]));
            EXPECT_EQ(table.Read(handles[j]).value, static_cast<int>(j));
        }
        table.Remove(handles[i]);
    }

    EXPECT_TRUE(table.IsEmpty());
}

TEST(PackedTable, ClearExpiresEveryHandle) {
    Table table;

    const auto handle = table.Add(Row{0});
    table.Clear();

    EXPECT_TRUE(table.IsEmpty());
    EXPECT_FALSE(table.IsValid(handle));
}
