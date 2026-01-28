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
            [passes.metal_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
            [passes.d3d_shader]
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
            [passes.metal_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
            [passes.d3d_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
        [[passes]]
        name = "pass2"
        inputs = ["target1"]
        targets = ["BackBuffer"]
            [passes.metal_shader]
            vs_file = "path3"
            vs_func = "function3"
            fs_file = "path4"
            fs_func = "function4"
            [passes.d3d_shader]
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

TEST(RenderParser, ParseSamplerPresetWithIndividualFilters){
    std::string tomlText = R"(
        [[sampler_presets]]
        name = "LINEAR_WRAP"
        minFilter = "Linear"
        magFilter = "Linear"
        mipFilter = "Linear"
        addressU = "Wrap"
        addressV = "Wrap"
        addressW = "Wrap"

        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["BackBuffer"]
        fs_samplers = [
            {preset = "LINEAR_WRAP"}
        ]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 1);
    ASSERT_EQ(render.passes[0].fs_samplers.size(), 1);

    const auto& sampler = render.passes[0].fs_samplers[0];
    EXPECT_EQ(sampler.minFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler.magFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler.mipFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler.addressU, RHIAddressMode::Wrap);
    EXPECT_EQ(sampler.addressV, RHIAddressMode::Wrap);
    EXPECT_EQ(sampler.addressW, RHIAddressMode::Wrap);
}

TEST(RenderParser, ParseSamplerPresetWithUnifiedFilter){
    std::string tomlText = R"(
        [[sampler_presets]]
        name = "NEAREST_CLAMP"
        filter = "Nearest"
        address = "Clamp"

        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["BackBuffer"]
        fs_samplers = [
            {preset = "NEAREST_CLAMP"}
        ]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 1);
    ASSERT_EQ(render.passes[0].fs_samplers.size(), 1);

    const auto& sampler = render.passes[0].fs_samplers[0];
    EXPECT_EQ(sampler.minFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler.magFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler.mipFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler.addressU, RHIAddressMode::Clamp);
    EXPECT_EQ(sampler.addressV, RHIAddressMode::Clamp);
    EXPECT_EQ(sampler.addressW, RHIAddressMode::Clamp);
}

TEST(RenderParser, ParseMultipleSamplerPresets){
    std::string tomlText = R"(
        [[sampler_presets]]
        name = "LINEAR_WRAP"
        filter = "Linear"
        address = "Wrap"
        [[sampler_presets]]
        name = "NEAREST_CLAMP"
        filter = "Nearest"
        address = "Clamp"

        [[render_targets]]
        name = "target1"
        format = "RGBA8_UNORM"
        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["target1"]
        fs_samplers = [
            {preset = "LINEAR_WRAP"}
        ]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
        [[passes]]
        name = "pass2"
        inputs = ["target1"]
        targets = ["BackBuffer"]
        fs_samplers = [
            {preset = "NEAREST_CLAMP"}
        ]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 2);

    ASSERT_EQ(render.passes[0].fs_samplers.size(), 1);
    EXPECT_EQ(render.passes[0].fs_samplers[0].minFilter, RHIFilter::Linear);
    EXPECT_EQ(render.passes[0].fs_samplers[0].addressU, RHIAddressMode::Wrap);

    ASSERT_EQ(render.passes[1].fs_samplers.size(), 1);
    EXPECT_EQ(render.passes[1].fs_samplers[0].minFilter, RHIFilter::Nearest);
    EXPECT_EQ(render.passes[1].fs_samplers[0].addressU, RHIAddressMode::Clamp);
}

TEST(RenderParser, ParseMultipleSamplersInSinglePass){
    std::string tomlText = R"(
        [[sampler_presets]]
        name = "LINEAR_WRAP"
        filter = "Linear"
        address = "Wrap"
        [[sampler_presets]]
        name = "NEAREST_CLAMP"
        filter = "Nearest"
        address = "Clamp"

        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["BackBuffer"]
        fs_samplers = [
            {preset = "LINEAR_WRAP"},
            {preset = "NEAREST_CLAMP"}
        ]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 1);
    ASSERT_EQ(render.passes[0].fs_samplers.size(), 2);

    EXPECT_EQ(render.passes[0].fs_samplers[0].minFilter, RHIFilter::Linear);
    EXPECT_EQ(render.passes[0].fs_samplers[0].addressU, RHIAddressMode::Wrap);

    EXPECT_EQ(render.passes[0].fs_samplers[1].minFilter, RHIFilter::Nearest);
    EXPECT_EQ(render.passes[0].fs_samplers[1].addressU, RHIAddressMode::Clamp);
}

TEST(RenderParser, IncompletePresetIsIgnored){
    std::string tomlText = R"(
        [[sampler_presets]]
        filter = "Linear"
        address = "Wrap"
        [[sampler_presets]]
        name = "VALID_PRESET"
        filter = "Linear"
        address = "Wrap"

        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["BackBuffer"]
        fs_samplers = [
            {preset = "VALID_PRESET"}
        ]
            [passes.metal_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 1);
    ASSERT_EQ(render.passes[0].fs_samplers.size(), 1);
    EXPECT_EQ(render.passes[0].fs_samplers[0].minFilter, RHIFilter::Linear);
}

TEST(RenderParser, NonexistentPresetIsIgnored){
    std::string tomlText = R"(
        [[sampler_presets]]
        name = "VALID_PRESET"
        filter = "Linear"
        address = "Wrap"

        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
        targets = ["BackBuffer"]
        fs_samplers = [
            {preset = "NONEXISTENT"},
            {preset = "VALID_PRESET"}
        ]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_THAT(render.passes.size(), 1);
    ASSERT_THAT(render.passes[0].fs_samplers.size(), 1);
    EXPECT_EQ(render.passes[0].fs_samplers[0].minFilter, RHIFilter::Linear);
}

TEST(RenderParser, PassWithoutSamplers){
    std::string tomlText = R"(
        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "noSamplerPass"
        targets = ["BackBuffer"]
            [passes.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.passes.size(), 1);
    EXPECT_TRUE(render.passes[0].fs_samplers.empty());
}