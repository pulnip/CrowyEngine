#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "ConfigParser.hpp"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::SizeIs;

using namespace Crowy;

TEST(ScriptParser, ParseSimpleModule){
    std::string tomlText = R"(
        [[modules]]
        name = "Module1"
        files = [
            "file1",
            "file2",
            "file3"
        ]
    )";
    auto script = parseScriptFromString(tomlText);

    ASSERT_THAT(script.modules, SizeIs(1));
    EXPECT_THAT(script.modules, Contains(AllOf(
        Field(&ScriptModuleSpec::name, Eq("Module1")),
        Field(&ScriptModuleSpec::files, ElementsAre(
            "file1", "file2", "file3"
        ))
    )));
}