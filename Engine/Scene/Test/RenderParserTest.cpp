#include <gtest/gtest.h>
#include "RenderParser.hpp"

using namespace Crowy;

TEST(SceneParser, ParseSimplePass){
    std::string tomlText = R"(
        [[passes]]
        name = "forward_opaque"
        [passes.shader]
        vs_file = "path1"
        vs_func = "function1"
        fs_file = "path2"
        fs_func = "function2"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 1);
    EXPECT_EQ(render.passes[0].name, "forward_opaque");
    EXPECT_EQ(render.passes[0].shader.vsFilePath, "path1");
    EXPECT_EQ(render.passes[0].shader.vsFuncName, "function1");
    EXPECT_EQ(render.passes[0].shader.fsFilePath, "path2");
    EXPECT_EQ(render.passes[0].shader.fsFuncName, "function2");
}