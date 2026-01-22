#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "RenderParser.hpp"

using ::testing::AllOf;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Field;
using ::testing::IsEmpty;
using ::testing::Pair;
using ::testing::SizeIs;

using namespace Crowy;

TEST(RenderParser, ParseSimplePass){
    std::string tomlText = R"(
        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"
        [[passes]]
        name = "pass1"
        targets = ["BackBuffer"]
            [passes.shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_THAT(render.renderTargets, SizeIs(1));
    EXPECT_THAT(render.renderTargets, Contains(Pair(
        Eq("BackBuffer"),
        Field(&RHITextureCreateDesc::format, Eq(RHITextureFormat::RGBA8_UNORM))
    )));

    ASSERT_THAT(render.passes, SizeIs(1));
    EXPECT_THAT(render.passes, Contains(AllOf(
        Field(&RenderPassSpec::name, Eq("pass1")),
        Field(&RenderPassSpec::targets, ElementsAre("BackBuffer")),
        Field(&RenderPassSpec::shader, AllOf(
            Field(&ShaderSpec::vsFilePath, Eq("path1")),
            Field(&ShaderSpec::vsFuncName, Eq("function1")),
            Field(&ShaderSpec::fsFilePath, Eq("path2")),
            Field(&ShaderSpec::fsFuncName, Eq("function2"))
        ))
    )));
}

TEST(RenderParser, ParseMultiplePasses){
    std::string tomlText = R"(
        [[render_targets]]
        name = "target1"
        format = "RGBA8_UNORM"
        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["target1"]
        [passes.shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
        [[passes]]
        name = "pass2"
        inputs = ["target1"]
        targets = ["BackBuffer"]
            [passes.shader]
            vs_file = "path3"
            vs_func = "function3"
            fs_file = "path4"
            fs_func = "function4"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_THAT(render.renderTargets, SizeIs(2));
    EXPECT_THAT(render.renderTargets, Contains(Pair(
        Eq("target1"),
        Field(&RHITextureCreateDesc::format, Eq(RHITextureFormat::RGBA8_UNORM))
    )));
    EXPECT_THAT(render.renderTargets, Contains(Pair(
        Eq("BackBuffer"),
        Field(&RHITextureCreateDesc::format, Eq(RHITextureFormat::RGBA8_UNORM))
    )));

    ASSERT_THAT(render.passes, SizeIs(2));
    EXPECT_THAT(render.passes, ElementsAre(
        AllOf(
            Field(&RenderPassSpec::name, Eq("pass1")),
            Field(&RenderPassSpec::targets, ElementsAre("target1")),
            Field(&RenderPassSpec::inputs, IsEmpty()),
            Field(&RenderPassSpec::shader, AllOf(
                Field(&ShaderSpec::vsFilePath, Eq("path1")),
                Field(&ShaderSpec::vsFuncName, Eq("function1")),
                Field(&ShaderSpec::fsFilePath, Eq("path2")),
                Field(&ShaderSpec::fsFuncName, Eq("function2"))
            ))
        ),
        AllOf(
            Field(&RenderPassSpec::name, Eq("pass2")),
            Field(&RenderPassSpec::inputs, ElementsAre("target1")),
            Field(&RenderPassSpec::targets, ElementsAre("BackBuffer")),
            Field(&RenderPassSpec::shader, AllOf(
                Field(&ShaderSpec::vsFilePath, Eq("path3")),
                Field(&ShaderSpec::vsFuncName, Eq("function3")),
                Field(&ShaderSpec::fsFilePath, Eq("path4")),
                Field(&ShaderSpec::fsFuncName, Eq("function4"))
            ))
        )
    ));
}