#include <vector>

#include <gtest/gtest.h>

#include "EditorPanels.hpp"

using namespace Crowy;

namespace
{
    std::vector<EditorObject> streetObjects() {
        return {
            EditorObject{.name = "instance/lamp-ne", .group = "Street", .detail = "StreetLamp"},
            EditorObject{.name = "instance/tower", .group = "NE", .detail = "BoxGrey"},
            EditorObject{.name = "instance/lamp-sw", .group = "Street", .detail = "StreetLamp"},
            EditorObject{
                .name = "light/lamp-ne",
                .group = "Lights · street_lamps",
                .detail = "spot",
                .kind = EditorObjectKind::Light
            },
        };
    }
}

TEST(EditorPanels, FoldersKeepTheContentsOrder) {
    const auto objects = streetObjects();
    const auto groups = groupObjects(objects);

    ASSERT_EQ(groups.size(), 3u);
    EXPECT_EQ(groups[0].name, "Street");
    EXPECT_EQ(groups[0].objects, (std::vector<usize>{0, 2}));
    EXPECT_EQ(groups[1].name, "NE");
    EXPECT_EQ(groups[2].name, "Lights · street_lamps");
    EXPECT_EQ(groups[2].objects, std::vector<usize>{3});
}

TEST(EditorPanels, SearchReadsNamesAndModelsIgnoringCase) {
    const auto objects = streetObjects();

    EXPECT_EQ(filterObjects(objects, "LAMP"), (std::vector<usize>{0, 2, 3}));
    EXPECT_EQ(filterObjects(objects, "boxgrey"), std::vector<usize>{1});
    EXPECT_EQ(filterObjects(objects, "lamp-ne"), (std::vector<usize>{0, 3}));
    EXPECT_EQ(filterObjects(objects, "").size(), objects.size());
    EXPECT_TRUE(filterObjects(objects, "crane").empty());
}
