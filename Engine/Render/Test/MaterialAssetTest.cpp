#include <filesystem>
#include <format>
#include <fstream>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "FileUtil.hpp"
#include "ImageLoader.hpp"
#include "ImageWriter.hpp"
#include "JsonLoader.hpp"
#include "MaterialAsset.hpp"
#include "PathUtil.hpp"
#include "RenderScene.hpp"
#include "StandardFrame.hpp"
#include "TextureAsset.hpp"

using namespace Crowy;

namespace
{
    const auto engineRoot = toPath(CROWY_ENGINE_ROOT);

    AssetRef parsed(StrView text) {
        return AssetRef::Parse(text).value();
    }

    Str materialText(StrView body) {
        return std::format(
            R"({{"metadata":{{"version":2,"type":"material"}}{}}})",
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
        FakeDevice device;
        TextureStore textures{
            [this](const AssetRef& ref) {
                return loadTextureAsset(device, roots, ref);
            },
            [this](TextureAsset asset) {
                releaseTextureAsset(device, std::move(asset));
            }
        };
        RenderScene scene;
        MaterialStore materials{
            [this](const AssetRef& ref) {
                return loadMaterialAsset(scene, textures, roots, ref);
            },
            [this](MaterialAsset asset) {
                releaseMaterialAsset(scene, std::move(asset));
            }
        };

        void SetUp() override { std::filesystem::create_directories(folder); }
        void TearDown() override { std::filesystem::remove_all(folder); }

        AssetRef written(StrView name, StrView text) {
            std::ofstream(folder / name, std::ios::binary) << text;
            return parsed(name);
        }

        void writeTexture(StrView name, u32 size) {
            const std::vector<u8> pixels(
                static_cast<usize>(size) * size * 4,
                200
            );
            writePng(
                folder / name,
                Rgba8View{
                    .pixels = pixels.data(),
                    .width = size,
                    .height = size,
                    .rowPitch = static_cast<usize>(size) * 4
                },
                true
            );
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
    std::vector<std::filesystem::path> paths{
        engineRoot / "Scene/Sample/Brass.json",
        engineRoot / "Scene/Sample/Chalk.json"
    };
    for(const auto& entry: std::filesystem::directory_iterator(
            engineRoot / "Render/Sample/Playground"
        ))
        paths.push_back(entry.path());

    for(const auto& path: paths) {
        SCOPED_TRACE(toUTF8String(path));
        auto document = parseJsonFile(path);
        if(document.get<Str>("metadata.type") != "material")
            continue;

        const auto desc = parseMaterialDesc(std::move(document));
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
        {R"(,"albedoMap":{"image":"a.png"})",
         "no property 'albedoMap.image' on 'MaterialTexture'"},
        {R"(,"emissiveMap":{"sampler":"Anisotropic"})",
         "'emissiveMap.sampler' expects TextureSampler (one of LinearWrap, "
         "LinearClamp, "
         "LinearMirror, LinearBorder, NearestWrap, NearestClamp, "
         "NearestMirror, NearestBorder)"},
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
        load(R"({"metadata":{"version":1,"type":"material"}})").error(),
        "Broken.json: a material of version 1 cannot be read; this reads "
        "version 2"
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

namespace
{
    FakeTexture& fakeOf(const TextureAssetHandle& asset) {
        return static_cast<FakeTexture&>(*asset->texture);
    }

    constexpr CStr SlotsBody =
        R"(,"albedoMap":{"texture":"Grid.png","sampler":"NearestClamp"},)"
        R"("emissiveMap":{"texture":"Glow.png","sampler":"LinearMirror"})";
}

// the slot decides the sampler and owns the row; the asset owns the texture
TEST_F(MaterialAssetTest, ASlotsRowSamplesItsTextureWithTheSlotsSampler) {
    writeTexture("Grid.png", 8);
    writeTexture("Glow.png", 4);

    const auto material =
        materials.Load(written("Lit.json", materialText(SlotsBody))).value();
    const auto& row = scene.Materials().GetRef(material->row);
    ASSERT_TRUE(scene.Textures().IsValid(row.maps.albedo));
    ASSERT_TRUE(scene.Textures().IsValid(row.maps.emissive));

    const auto& albedo = scene.Textures().GetRef(row.maps.albedo);
    EXPECT_EQ(albedo.sampler, TextureSampler::NearestClamp);
    EXPECT_EQ(albedo.texture, nullptr);
    EXPECT_EQ(albedo.readable, FakeDevice::FirstTextureID);

    const auto& emissive = scene.Textures().GetRef(row.maps.emissive);
    EXPECT_EQ(emissive.sampler, TextureSampler::LinearMirror);
    EXPECT_EQ(emissive.readable, FakeDevice::FirstTextureID + 1);
}

// a slot written by name comes back as written, in declaration order
TEST_F(MaterialAssetTest, TheSlotsSaveBackAsTheyRead) {
    MaterialDesc desc;
    desc.albedoMap = {parsed("engine:Wood.png"), TextureSampler::NearestWrap};

    const auto text = emitJson(serializeMaterialDesc(desc), JsonStyle::Pretty);
    EXPECT_LT(text.find("\"albedoMap\""), text.find("\"emissiveMap\""));
    EXPECT_NE(text.find("\"sampler\": \"NearestWrap\""), Str::npos);

    const auto reread = parseMaterialDesc(parseJsonString(text));
    ASSERT_TRUE(reread.has_value()) << reread.error();
    EXPECT_EQ(reread->albedoMap.texture, desc.albedoMap.texture);
    EXPECT_EQ(reread->albedoMap.sampler, TextureSampler::NearestWrap);
    EXPECT_TRUE(reread->emissiveMap.texture.IsNone());
    EXPECT_EQ(
        emitJson(serializeMaterialDesc(*reread), JsonStyle::Pretty),
        text
    );
}

// the file keeps the path that failed, so a save writes it back
TEST_F(MaterialAssetTest, ATextureThatCannotLoadIsSampledAsMissing) {
    const auto body = R"(,"albedoMap":{"texture":"Gone.png"})";
    const auto material =
        materials.Load(written("Lost.json", materialText(body))).value();

    EXPECT_EQ(
        material->albedo.asset.Ref().ToString(),
        "engine:Builtin/Missing"
    );
    const auto& row = scene.Materials().GetRef(material->row);
    EXPECT_TRUE(scene.Textures().IsValid(row.maps.albedo));
    EXPECT_FALSE(scene.Textures().IsValid(row.maps.emissive));

    const auto desc = parseMaterialDesc(parseJsonFile(folder / "Lost.json"));
    EXPECT_EQ(desc->albedoMap.texture, parsed("Gone.png"));

    const auto error = textures.Load(parsed("Gone.png")).error();
    EXPECT_TRUE(error.starts_with("Gone.png: ")) << error;
}

// two materials naming one path hold one asset, which leaves at the Collect
// after the last of them
TEST_F(MaterialAssetTest, MaterialsNamingOnePathShareItsAsset) {
    writeTexture("Grid.png", 8);
    const auto body = R"(,"albedoMap":{"texture":"Grid.png"})";
    auto first = materials.Load(written("A.json", materialText(body))).value();
    auto second = materials.Load(written("B.json", materialText(body))).value();
    EXPECT_EQ(textures.Count(), 1u);
    EXPECT_EQ(device.textureCreates.size(), 1u);
    EXPECT_EQ(scene.Textures().Count(), 2u);

    first = {};
    materials.Collect();
    textures.Collect();
    EXPECT_EQ(textures.Count(), 1u);
    EXPECT_EQ(scene.Textures().Count(), 1u);

    second = {};
    materials.Collect();
    EXPECT_EQ(textures.Count(), 1u);
    textures.Collect();
    EXPECT_EQ(textures.Count(), 0u);
    EXPECT_TRUE(scene.Textures().IsEmpty());

    EXPECT_EQ(device.texturesDestroyed, 0u);
    device.RunDeferred();
    EXPECT_EQ(device.texturesDestroyed, 1u);
}

// a nearest sampler still walks the chain it is given, so its slot is given
// one mip
TEST_F(MaterialAssetTest, ANearestSlotReadsOneMipAndALinearSlotTheChain) {
    writeTexture("Grid.png", 8);
    const auto body =
        R"(,"albedoMap":{"texture":"Grid.png","sampler":"NearestClamp"},)"
        R"("emissiveMap":{"texture":"Grid.png","sampler":"LinearClamp"})";
    const auto material =
        materials.Load(written("Both.json", materialText(body))).value();

    ASSERT_EQ(device.textureCreates.size(), 1u);
    EXPECT_EQ(device.textureCreates[0].mipLevels, 4u);
    EXPECT_EQ(device.textureCreates[0].initialData.size(), 4u);

    const std::vector<RHITextureViewDesc> views{
        {.format = RHIPixelFormat::RGBA8_UNORM_SRGB, .mipCount = 1},
        {.format = RHIPixelFormat::RGBA8_UNORM_SRGB, .mipCount = AllMips}
    };
    EXPECT_EQ(fakeOf(material->albedo.asset).viewsRead, views);
}

TEST_F(MaterialAssetTest, AKtx2KeepsTheMipsOfItsFile) {
    const AssetRoots contentRoots{
        .engine = engineRoot,
        .content = engineRoot.parent_path() / "Content"
    };
    const auto ref = parsed("Assets/blender_uv_grid_2k.ktx2");

    const auto image = LoadImage(resolveAssetPath(contentRoots, ref));
    ASSERT_GT(image.mipLevels, 1u);

    const auto asset = loadTextureAsset(device, contentRoots, ref);
    ASSERT_TRUE(asset.has_value()) << asset.error();
    ASSERT_EQ(device.textureCreates.size(), 1u);
    EXPECT_EQ(device.textureCreates[0].mipLevels, image.mipLevels);
    EXPECT_EQ(device.textureCreates[0].format, image.format);
}

TEST_F(MaterialAssetTest, TheBuiltinTexturesAreMadeByCode) {
    const auto load = [&](StrView name) {
        const auto asset = textures.Load(parsed(name)).value();
        return device.textureCreates.back();
    };

    const auto white = load("engine:Builtin/White");
    EXPECT_EQ(white.format, RHIPixelFormat::RGBA8_UNORM_SRGB);
    EXPECT_EQ(white.mipLevels, 1u);

    const auto flat = load("engine:Builtin/FlatNormal");
    EXPECT_EQ(flat.format, RHIPixelFormat::RGBA8_UNORM);

    const auto missing = load("engine:Builtin/Missing");
    EXPECT_EQ(missing.width, 2u);
    EXPECT_EQ(missing.mipLevels, 2u);

    EXPECT_EQ(
        textures.Load(parsed("engine:Builtin/Black")).error(),
        "engine:Builtin/Black: no built-in texture 'Black'; the built-ins "
        "are White, FlatNormal, Missing"
    );
}

TEST(TextureView, AColorSpaceNamesTheFormatTheTexelsAreReadAs) {
    using enum RHIPixelFormat;
    using enum TextureColorSpace;
    const auto sampler = TextureSampler::LinearWrap;

    EXPECT_EQ(
        readViewOf(RGBA8_UNORM_SRGB, Srgb, sampler).format,
        RGBA8_UNORM_SRGB
    );
    EXPECT_EQ(
        readViewOf(RGBA8_UNORM_SRGB, Linear, sampler).format,
        RGBA8_UNORM
    );
    EXPECT_EQ(readViewOf(BC7_UNORM, Srgb, sampler).format, BC7_UNORM_SRGB);
    EXPECT_EQ(readViewOf(R8_UNORM, Srgb, sampler).format, R8_UNORM);
}
