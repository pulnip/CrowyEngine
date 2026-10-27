#include <filesystem>
#include <stdexcept>

#include <gtest/gtest.h>

#include "JsonLoader.hpp"
#include "ProjectFile.hpp"

using namespace Crowy;

namespace
{
    ProjectFile parse(StrView json) {
        return DomTraits<ProjectFile>::from(parseJsonString(json));
    }
}

TEST(ProjectFile, ContentNamesAFolderInsideTheProject) {
    EXPECT_EQ(parse(R"({"content": "Content"})").content, "Content");
    EXPECT_EQ(
        parse(R"({"content": "Game/./Content/"})").content,
        std::filesystem::path("Game/Content/")
    );
}

TEST(ProjectFile, AnUnknownKeyIsAnError) {
    EXPECT_THROW(
        parse(R"({"content": "Content", "boot": {}})"),
        std::runtime_error
    );
}

TEST(ProjectFile, ContentIsRequired) {
    EXPECT_THROW(parse(R"({})"), std::runtime_error);
    EXPECT_THROW(parse(R"({"content": ""})"), std::runtime_error);
    EXPECT_THROW(parse(R"({"content": 1})"), std::runtime_error);
    EXPECT_THROW(parse(R"([])"), std::runtime_error);
}

TEST(ProjectFile, ContentStaysInsideTheProject) {
    EXPECT_THROW(parse(R"({"content": "../Backlot"})"), std::runtime_error);
    EXPECT_THROW(parse(R"({"content": "a/../../b"})"), std::runtime_error);
    EXPECT_THROW(parse(R"({"content": "/Content"})"), std::runtime_error);
#ifdef _WIN32
    EXPECT_THROW(parse(R"({"content": "C:/Content"})"), std::runtime_error);
#endif
}

TEST(ProjectFile, AnAbsolutePathIsKept) {
    const auto absolute = std::filesystem::temp_directory_path() / "a.png";
    EXPECT_EQ(resolveContentPath(absolute), absolute);
}
