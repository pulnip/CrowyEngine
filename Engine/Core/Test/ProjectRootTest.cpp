#include <filesystem>
#include <fstream>
#include <string>

#include <gtest/gtest.h>

#include "ProjectRoot.hpp"

using namespace Crowy;

namespace
{
    // a fresh folder under the temp directory, removed with the fixture
    class ProjectRootTest: public testing::Test {
    protected:
        std::filesystem::path base;

        void SetUp() override {
            const auto* test =
                testing::UnitTest::GetInstance()->current_test_info();
            base = std::filesystem::temp_directory_path() /
                   "CrowyProjectRootTest" / test->name();
            std::filesystem::remove_all(base);
            std::filesystem::create_directories(base);
        }

        void TearDown() override { std::filesystem::remove_all(base); }

        void writeProjectFile(const std::filesystem::path& folder) {
            std::filesystem::create_directories(folder);
            std::ofstream(folder / ProjectFileName) << "{}\n";
        }
    };
}

TEST_F(ProjectRootTest, TheStartFolderItselfIsARoot) {
    writeProjectFile(base);
    EXPECT_EQ(findProjectRoot(base), base.lexically_normal());
}

TEST_F(ProjectRootTest, TheNearestFolderAboveWins) {
    writeProjectFile(base);
    writeProjectFile(base / "inner");
    const auto start = base / "inner" / "build" / "bin" / "Debug";
    std::filesystem::create_directories(start);

    EXPECT_EQ(findProjectRoot(start), (base / "inner").lexically_normal());
}

TEST_F(ProjectRootTest, AFolderNamedLikeTheFileIsNotARoot) {
    std::filesystem::create_directories(base / ProjectFileName);
    const auto found = findProjectRoot(base);
    EXPECT_NE(found, base.lexically_normal());
}

TEST_F(ProjectRootTest, AnAbsolutePathIsKept) {
    EXPECT_EQ(resolveProjectPath(base / "a.png"), base / "a.png");
}
