#include <filesystem>
#include <format>
#include <fstream>

#include <gtest/gtest.h>

#include "FileUtil.hpp"
#include "JsonLoader.hpp"
#include "MaterialAsset.hpp"
#include "PathUtil.hpp"
#include "RenderScene.hpp"
#include "StandardFrame.hpp"

using namespace Crowy;

namespace
{
    const auto engineRoot = toPath(CROWY_ENGINE_ROOT);

    AssetRef parsed(StrView text) {
        return AssetRef::Parse(text).value();
    }

    Str materialText(StrView body) {
        return std::format(
            R"({{"metadata":{{"version":1,"type":"material"}}{}}})",
            body
        );
    }

    // the sample materials on the engine: mount, and files the test writes
    // on the content one
    class MaterialAssetTest: public testing::Test {
    protected:
        std::filesystem::path folder =
            std::filesystem::temp_directory_path() / "CrowyMaterialAssetTest";
        AssetRoots roots{.engine = engineRoot, .content = folder};
        RenderScene scene;
        MaterialStore materials{
            [this](const AssetRef& ref) {
                return loadMaterialAsset(scene.Materials(), roots, ref);
            },
            [this](MaterialAsset asset) {
                releaseMaterialAsset(scene.Materials(), asset);
            }
        };

        void SetUp() override { std::filesystem::create_directories(folder); }
        void TearDown() override { std::filesystem::remove_all(folder); }

        AssetRef written(StrView name, StrView text) {
            std::ofstream(folder / name, std::ios::binary) << text;
            return parsed(name);
        }

        const MaterialResource& rowOf(const AssetRef& ref) {
            loadedHandles.push_back(materials.Load(ref).value());
            return scene.Materials().GetRef(loadedHandles.back()->row);
        }

    private:
        MaterialAssetHandles loadedHandles;
    };
}

// a key a file gives that never reaches the row reads as its default
TEST_F(MaterialAssetTest, AFilesValuesReachItsRow) {
    const auto& brass = rowOf(parsed("engine:Scene/Sample/Brass.json"));
    EXPECT_EQ(brass.data.albedo, (Vec3{0.9f, 0.7f, 0.3f}));
    EXPECT_EQ(brass.data.metallic, 1.0f);
    EXPECT_EQ(brass.data.roughness, 0.3f);
    EXPECT_EQ(brass.pipeline.domain, MaterialDomain::Opaque);
    EXPECT_EQ(brass.pipeline.shadingModule, toPath(PBRShadingModule));
    EXPECT_EQ(brass.pipeline.rasterizer.cullMode, RHICullMode::Back);
    EXPECT_EQ(brass.data.flags, 0u);

    const auto& chalk = rowOf(parsed("engine:Scene/Sample/Chalk.json"));
    EXPECT_EQ(chalk.pipeline.shadingModule, engineRoot / "Shader/Unlit.slang");
    EXPECT_EQ(
        chalk.data.flags,
        static_cast<u32>(MaterialFlags::NoShadowReceive)
    );

    const auto& cutout = rowOf(written(
        "Cutout.json",
        materialText(
            R"(,"domain":"Masked","doubleSided":true,)"
            R"("shader":"engine:Render/Shader/StandardForward.slang")"
        )
    ));
    const auto shader = engineRoot / "Render/Shader/StandardForward.slang";
    EXPECT_EQ(cutout.pipeline.domain, MaterialDomain::Masked);
    EXPECT_EQ(cutout.pipeline.rasterizer.cullMode, RHICullMode::None);
    EXPECT_EQ(
        cutout.pipeline.vertexShader,
        (RHIShaderDesc{.path = shader, .entryPoint = "vs_main"})
    );
    EXPECT_EQ(
        cutout.pipeline.fragmentShader,
        (RHIShaderDesc{.path = shader, .entryPoint = "fs_main"})
    );
    EXPECT_EQ(
        cutout.pipeline.maskShader,
        (RHIShaderDesc{.path = shader, .entryPoint = "fs_masked_depth"})
    );
}

TEST_F(MaterialAssetTest, TheSampleFilesSaveBackAsTheyRead) {
    for(const auto* name: {"Brass.json", "Chalk.json"}) {
        SCOPED_TRACE(name);
        const auto path = engineRoot / "Scene/Sample" / name;
        const auto desc = parseMaterialDesc(parseJsonFile(path));
        ASSERT_TRUE(desc.has_value()) << desc.error();

        EXPECT_EQ(
            emitJson(serializeMaterialDesc(*desc), JsonStyle::Pretty) + "\n",
            readFileAsString(path)
        );
    }
}

TEST_F(MaterialAssetTest, AFileThatCannotLoadSaysWhereAndWhy) {
    struct Broken {
        Str body;
        Str message;
    };
    const Broken cases[] = {
        {R"(,"shine":1)", "no property 'shine' on 'MaterialDesc'"},
        {R"(,"domain":"Glass")",
         "'domain' expects MaterialDomain (one of Opaque, Translucent, "
         "Masked)"},
        {R"(,"shader":"Gone.slang")",
         std::format(
             "the shader 'Gone.slang' is no file at {}",
             toUTF8String(folder / "Gone.slang")
         )},
    };

    const auto load = [&](StrView text) {
        return materials.Load(written("Broken.json", text));
    };
    for(const auto& [body, message]: cases) {
        SCOPED_TRACE(body);
        EXPECT_EQ(
            load(materialText(body)).error(),
            std::format("Broken.json: {}", message)
        );
    }
    EXPECT_EQ(
        load(R"({"metadata":{"version":1,"type":"scene"}})").error(),
        "Broken.json: expected a \"material\" document, got \"scene\""
    );
    EXPECT_EQ(
        load(R"({"metadata":{"version":2,"type":"material"}})").error(),
        "Broken.json: a material of version 2 cannot be read; this reads "
        "version 1"
    );
    EXPECT_EQ(scene.Materials().Count(), 0u);
}

// the built-ins are values of the file's struct, so a field a preset sets
// can fall off while the table is rewritten
TEST_F(MaterialAssetTest, TheBuiltinsKeepTheirValues) {
    const auto& translucent = rowOf(parsed("engine:Builtin/Translucent"));
    EXPECT_EQ(translucent.pipeline.domain, MaterialDomain::Translucent);
    EXPECT_EQ(translucent.data.opacity, 0.5f);
    EXPECT_TRUE(translucent.pipeline.blend.has_value());

    const auto& masked = rowOf(parsed("engine:Builtin/Masked"));
    EXPECT_EQ(masked.pipeline.domain, MaterialDomain::Masked);
    EXPECT_EQ(masked.pipeline.maskShader.path, toPath(StandardForwardShader));

    const auto& missing = rowOf(parsed("engine:Builtin/Missing"));
    EXPECT_EQ(missing.data.albedo, zeros());
    EXPECT_EQ(missing.data.emissive, (Vec3{1.0f, 0.0f, 1.0f}));
}
