#include "gmock/gmock.h"
#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include "ConfigParser.hpp"
#include "RenderSpec.hpp"

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
        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"
        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [passes.pipelines.metal_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
            [passes.pipelines.d3d_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_THAT(render.textures, SizeIs(1));
    EXPECT_THAT(render.textures, Contains(Pair(
        Eq("BackBuffer"),
        Field(&RHITextureCreateDesc::format, Eq(RHITextureFormat::RGBA8_UNORM))
    )));

    ASSERT_THAT(render.renderPasses, SizeIs(1));
    const auto& pass1 = render.renderPasses[0];
    EXPECT_EQ(pass1.name, "pass1");

    ASSERT_THAT(pass1.pipelines, SizeIs(1));
    EXPECT_THAT(pass1.pipelines, Contains(AllOf(
        Field(&GraphicsPipelineBindSpec::outputs, ElementsAre("BackBuffer")),
        Field(&GraphicsPipelineBindSpec::shader, AllOf(
            Field(&ShaderSpec::vsFilePath, Eq("path1")),
            Field(&ShaderSpec::vsFuncName, Eq("function1")),
            Field(&ShaderSpec::fsFilePath, Eq("path2")),
            Field(&ShaderSpec::fsFuncName, Eq("function2"))
        ))
    )));
}

TEST(RenderParser, ParseMultiplePasses){
    std::string tomlText = R"(
        [[textures]]
        name = "target1"
        format = "RGBA8_UNORM"
        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["target1"]
            [passes.pipelines.metal_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
            [passes.pipelines.d3d_shader]
            vs_file = "path1"
            vs_func = "function1"
            fs_file = "path2"
            fs_func = "function2"
        [[passes]]
        name = "pass2"
            [[passes.pipelines]]
            inputs = ["target1"]
            outputs = ["BackBuffer"]
            [passes.pipelines.metal_shader]
            vs_file = "path3"
            vs_func = "function3"
            fs_file = "path4"
            fs_func = "function4"
            [passes.pipelines.d3d_shader]
            vs_file = "path3"
            vs_func = "function3"
            fs_file = "path4"
            fs_func = "function4"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_THAT(render.textures, SizeIs(2));
    EXPECT_THAT(render.textures, Contains(Pair(
        Eq("target1"),
        Field(&RHITextureCreateDesc::format, Eq(RHITextureFormat::RGBA8_UNORM))
    )));
    EXPECT_THAT(render.textures, Contains(Pair(
        Eq("BackBuffer"),
        Field(&RHITextureCreateDesc::format, Eq(RHITextureFormat::RGBA8_UNORM))
    )));

    ASSERT_THAT(render.renderPasses, SizeIs(2));
    EXPECT_THAT(render.renderPasses, ElementsAre(
        AllOf(
            Field(&RenderPassSpec::name, Eq("pass1")),
            Field(&RenderPassSpec::pipelines, ElementsAre(
                AllOf(
                    Field(&GraphicsPipelineBindSpec::inputs, IsEmpty()),
                    Field(&GraphicsPipelineBindSpec::outputs, ElementsAre("target1")),
                    Field(&GraphicsPipelineBindSpec::shader, AllOf(
                        Field(&ShaderSpec::vsFilePath, Eq("path1")),
                        Field(&ShaderSpec::vsFuncName, Eq("function1")),
                        Field(&ShaderSpec::fsFilePath, Eq("path2")),
                        Field(&ShaderSpec::fsFuncName, Eq("function2"))
                    ))
                )
            ))
        ),
        AllOf(
            Field(&RenderPassSpec::name, Eq("pass2")),
            Field(&RenderPassSpec::pipelines, ElementsAre(
                AllOf(
                    Field(&GraphicsPipelineBindSpec::inputs, ElementsAre("target1")),
                    Field(&GraphicsPipelineBindSpec::outputs, ElementsAre("BackBuffer")),
                    Field(&GraphicsPipelineBindSpec::shader, AllOf(
                        Field(&ShaderSpec::vsFilePath, Eq("path3")),
                        Field(&ShaderSpec::vsFuncName, Eq("function3")),
                        Field(&ShaderSpec::fsFilePath, Eq("path4")),
                        Field(&ShaderSpec::fsFuncName, Eq("function4"))
                    ))
                )
            ))
        )
    ));
}

TEST(RenderParser, ParseSamplerWithIndividualFilters){
    std::string tomlText = R"(
        [[samplers]]
        name = "LINEAR_WRAP"
        minFilter = "Linear"
        magFilter = "Linear"
        mipFilter = "Linear"
        addressU = "Wrap"
        addressV = "Wrap"
        addressW = "Wrap"

        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [[passes.pipelines.fs_samplers]]
            name = "LINEAR_WRAP"
            slot = 0
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 1);
    const auto& pass1 = render.renderPasses[0];
    ASSERT_EQ(pass1.pipelines.size(), 1);
    const auto& pipeline = pass1.pipelines[0];
    ASSERT_EQ(pipeline.fs_samplers.size(), 1);
    const auto& samplerBind = pipeline.fs_samplers[0];

    const auto it = render.samplers.find(samplerBind.name);
    ASSERT_NE(it, render.samplers.end());
    const auto& sampler = it->second;
    EXPECT_EQ(sampler.minFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler.magFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler.mipFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler.addressU, RHIAddressMode::Wrap);
    EXPECT_EQ(sampler.addressV, RHIAddressMode::Wrap);
    EXPECT_EQ(sampler.addressW, RHIAddressMode::Wrap);
}

TEST(RenderParser, ParseSamplerWithUnifiedFilter){
    std::string tomlText = R"(
        [[samplers]]
        name = "NEAREST_CLAMP"
        filter = "Nearest"
        address = "Clamp"

        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [[passes.pipelines.fs_samplers]]
            name = "NEAREST_CLAMP"
            slot = 0
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 1);
    const auto& pass1 = render.renderPasses[0];
    ASSERT_EQ(pass1.pipelines.size(), 1);
    const auto& pipeline = pass1.pipelines[0];
    ASSERT_EQ(pipeline.fs_samplers.size(), 1);
    const auto& samplerBind = pipeline.fs_samplers[0];

    const auto it = render.samplers.find(samplerBind.name);
    ASSERT_NE(it, render.samplers.end());
    const auto& sampler = it->second;
    EXPECT_EQ(sampler.minFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler.magFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler.mipFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler.addressU, RHIAddressMode::Clamp);
    EXPECT_EQ(sampler.addressV, RHIAddressMode::Clamp);
    EXPECT_EQ(sampler.addressW, RHIAddressMode::Clamp);
}

TEST(RenderParser, ParseMultipleSamplers){
    std::string tomlText = R"(
        [[samplers]]
        name = "LINEAR_WRAP"
        filter = "Linear"
        address = "Wrap"
        [[samplers]]
        name = "NEAREST_CLAMP"
        filter = "Nearest"
        address = "Clamp"

        [[textures]]
        name = "target1"
        format = "RGBA8_UNORM"
        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["target1"]
            [[passes.pipelines.fs_samplers]]
            name = "LINEAR_WRAP"
            slot = 0
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
        [[passes]]
        name = "pass2"
            [[passes.pipelines]]
            inputs = ["target1"]
            outputs = ["BackBuffer"]
            [[passes.pipelines.fs_samplers]]
            name = "NEAREST_CLAMP"
            slot = 0
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 2);
    const auto& pass1 = render.renderPasses[0];
    const auto& pass2 = render.renderPasses[1];
    ASSERT_EQ(pass1.pipelines.size(), 1);
    const auto& pipeline1 = pass1.pipelines[0];
    ASSERT_EQ(pass2.pipelines.size(), 1);
    const auto& pipeline2 = pass2.pipelines[0];

    ASSERT_EQ(pipeline1.fs_samplers.size(), 1);
    const auto& samplerBind1 = pipeline1.fs_samplers[0];
    auto it1 = render.samplers.find(samplerBind1.name);
    ASSERT_NE(it1, render.samplers.end());
    const auto& sampler1 = it1->second;
    EXPECT_EQ(sampler1.minFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler1.addressU, RHIAddressMode::Wrap);

    ASSERT_EQ(pipeline2.fs_samplers.size(), 1);
    const auto& samplerBind2 = pipeline2.fs_samplers[0];
    auto it2 = render.samplers.find(samplerBind2.name);
    ASSERT_NE(it2, render.samplers.end());
    const auto& sampler2 = it2->second;
    EXPECT_EQ(sampler2.minFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler2.addressU, RHIAddressMode::Clamp);
}

TEST(RenderParser, ParseMultipleSamplersInSinglePass){
    std::string tomlText = R"(
        [[samplers]]
        name = "LINEAR_WRAP"
        filter = "Linear"
        address = "Wrap"
        [[samplers]]
        name = "NEAREST_CLAMP"
        filter = "Nearest"
        address = "Clamp"

        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [[passes.pipelines.fs_samplers]]
            name = "LINEAR_WRAP"
            slot = 0
            [[passes.pipelines.fs_samplers]]
            name = "NEAREST_CLAMP"
            slot = 1
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 1);
    const auto& pass = render.renderPasses[0];
    ASSERT_EQ(pass.pipelines.size(), 1);
    const auto& pipeline = pass.pipelines[0];
    ASSERT_EQ(pipeline.fs_samplers.size(), 2);
    const auto& samplerBind1 = pipeline.fs_samplers[0];
    const auto& samplerBind2 = pipeline.fs_samplers[1];

    auto it1 = render.samplers.find(samplerBind1.name);
    ASSERT_NE(it1, render.samplers.end());
    const auto& sampler1 = it1->second;
    EXPECT_EQ(sampler1.minFilter, RHIFilter::Linear);
    EXPECT_EQ(sampler1.addressU, RHIAddressMode::Wrap);

    auto it2 = render.samplers.find(samplerBind2.name);
    ASSERT_NE(it2, render.samplers.end());
    const auto& sampler2 = it2->second;
    EXPECT_EQ(sampler2.minFilter, RHIFilter::Nearest);
    EXPECT_EQ(sampler2.addressU, RHIAddressMode::Clamp);
}

TEST(RenderParser, IncompleteSamplerIsIgnored){
    std::string tomlText = R"(
        [[samplers]]
        filter = "Linear"
        address = "Wrap"
        [[samplers]]
        name = "VALID_PRESET"
        filter = "Linear"
        address = "Wrap"

        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [[passes.pipelines.fs_samplers]]
            name = "VALID_PRESET"
            slot = 0
            [passes.pipelines.metal_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 1);
    const auto& pass = render.renderPasses[0];
    ASSERT_EQ(pass.pipelines.size(), 1);
    const auto& pipeline = pass.pipelines[0];
    ASSERT_EQ(pipeline.fs_samplers.size(), 1);
    const auto& samplerBind = pipeline.fs_samplers[0];

    auto it = render.samplers.find(samplerBind.name);
    ASSERT_NE(it, render.samplers.end());
    const auto& sampler = it->second;
    EXPECT_EQ(sampler.minFilter, RHIFilter::Linear);
}

TEST(RenderParser, NonexistentSamplerIsIgnored){
    std::string tomlText = R"(
        [[samplers]]
        name = "VALID_PRESET"
        filter = "Linear"
        address = "Wrap"

        [[render_targets]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "pass1"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [[passes.pipelines.fs_samplers]]
            name = "NONEXISTENT"
            slot = 0
            [[passes.pipelines.fs_samplers]]
            name = "VALID_PRESET"
            slot = 1
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 1);
    const auto& pass = render.renderPasses[0];
    ASSERT_EQ(pass.pipelines.size(), 1);
    const auto& pipeline = pass.pipelines[0];
    ASSERT_EQ(pipeline.fs_samplers.size(), 1);
    const auto& samplerBind = pipeline.fs_samplers[0];

    auto it = render.samplers.find(samplerBind.name);
    ASSERT_NE(it, render.samplers.end());
    const auto& sampler = it->second;
    EXPECT_EQ(sampler.minFilter, RHIFilter::Linear);
}

TEST(RenderParser, PassWithoutSamplers){
    std::string tomlText = R"(
        [[textures]]
        name = "BackBuffer"
        format = "RGBA8_UNORM"

        [[passes]]
        name = "noSamplerPass"
            [[passes.pipelines]]
            outputs = ["BackBuffer"]
            [passes.pipelines.metal_shader]
            vs_file = "vs.metal"
            vs_func = "vs_main"
            fs_file = "fs.metal"
            fs_func = "fs_main"
            [passes.pipelines.d3d_shader]
            vs_file = "vs.hlsl"
            vs_func = "vs_main"
            fs_file = "fs.hlsl"
            fs_func = "fs_main"
    )";
    auto render = parseRenderFromString(tomlText);

    ASSERT_EQ(render.renderPasses.size(), 1);
    const auto& pass = render.renderPasses[0];
    ASSERT_EQ(pass.pipelines.size(), 1);
    const auto& pipeline = pass.pipelines[0];
    EXPECT_TRUE(pipeline.fs_samplers.empty());
}