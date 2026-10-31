#include <gtest/gtest.h>

#include "GenericHandle.hpp"
#include "HandleMap.hpp"

using namespace Crowy;

namespace
{
    struct RowTag;
    using Row = GenericHandle<RowTag>;
}

TEST(HandleMap, AHandleItsSlotNoLongerHoldsFindsNothing) {
    HandleMap<Row, int> values;
    values.Set(Row{2, 0}, 7);
    EXPECT_EQ(values.Find(Row{2, 0}), 7);
    EXPECT_FALSE(values.Find(Row{}));

    values.Set(Row{2, 1}, 8);
    EXPECT_FALSE(values.Find(Row{2, 0}));
    EXPECT_EQ(values.Find(Row{2, 1}), 8);
}

TEST(HandleMap, ErasingAnOldHandleKeepsTheNewcomer) {
    HandleMap<Row, int> values;
    values.Set(Row{0, 3}, 7);
    values.Set(Row{0, 4}, 8);

    values.Erase(Row{0, 3});
    EXPECT_EQ(values.Find(Row{0, 4}), 8);

    values.Erase(Row{0, 4});
    EXPECT_FALSE(values.Find(Row{0, 4}));
}
