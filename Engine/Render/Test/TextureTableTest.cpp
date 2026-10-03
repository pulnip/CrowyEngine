#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

using namespace Crowy;

namespace
{
    struct Fixture {
        FakeDevice device;
        SceneRenderer renderer{device, SceneRendererDesc{}};
        RenderScene scene;

        TextureHandle AddTexture(TextureSampler sampler) {
            return scene.Textures().Add(TextureResource{
                .texture = device.CreateTexture(
                    RHITextureCreateDesc{.width = 4, .height = 4},
                    "fake"
                ),
                .sampler = sampler
            });
        }

        MaterialHandle AddMaterial(MaterialMaps maps) {
            return scene.Materials().Add(MaterialResource{.maps = maps});
        }

        // the frame's rows as the shaders would read them
        ScenePush Frame() {
            renderer.BeginFrame(scene);
            renderer.Upload();

            return renderer.FramePush();
        }

        MaterialData MaterialRow(const ScenePush& push, MaterialHandle handle) const {
            const auto row = push.materialBase + scene.Materials().IndexOf(handle);

            return device.transient.Read<MaterialData>(
                static_cast<u32>(row * sizeof(MaterialData))
            );
        }

        TextureData TextureRow(const ScenePush& push, u32 mapID) const {
            return device.transient.Read<TextureData>(
                static_cast<u32>((push.textureBase + mapID - 1) * sizeof(TextureData))
            );
        }
    };
}

// map ID n names row n - 1 of the frame's table, and the row carries the
// texture's handle and its sampler
TEST(TextureTable, MapIDsNameTheFramesRows) {
    Fixture f;
    const auto palette = f.AddTexture(TextureSampler::NearestClamp);
    const auto atlas = f.AddTexture(TextureSampler::LinearClamp);
    const auto textured = f.AddMaterial({.albedo = palette});
    const auto glowing = f.AddMaterial({.albedo = atlas, .emissive = palette});
    const auto plain = f.AddMaterial({});

    const auto push = f.Frame();
    EXPECT_EQ(push.textures, FakeBuffer::ReadableID);

    EXPECT_EQ(f.MaterialRow(push, textured).albedoMapID, 1u);
    EXPECT_EQ(f.MaterialRow(push, textured).emissiveMapID, 0u);
    EXPECT_EQ(f.MaterialRow(push, glowing).albedoMapID, 2u);
    EXPECT_EQ(f.MaterialRow(push, glowing).emissiveMapID, 1u);
    EXPECT_EQ(f.MaterialRow(push, plain).albedoMapID, 0u);
    EXPECT_EQ(f.MaterialRow(push, plain).emissiveMapID, 0u);

    const auto first = f.TextureRow(push, 1);
    EXPECT_EQ(first.texture, FakeDevice::FirstTextureID);
    EXPECT_EQ(first.sampler, static_cast<u32>(TextureSampler::NearestClamp));
    const auto second = f.TextureRow(push, 2);
    EXPECT_EQ(second.texture, FakeDevice::FirstTextureID + 1);
    EXPECT_EQ(second.sampler, static_cast<u32>(TextureSampler::LinearClamp));
}

// a removed texture reads as none, and a moved one is followed
TEST(TextureTable, ARemovedTextureReadsAsNone) {
    Fixture f;
    const auto palette = f.AddTexture(TextureSampler::NearestClamp);
    const auto atlas = f.AddTexture(TextureSampler::LinearClamp);
    const auto textured = f.AddMaterial({.albedo = palette});
    const auto glowing = f.AddMaterial({.albedo = atlas, .emissive = palette});

    f.scene.Textures().Remove(palette);
    const auto push = f.Frame();

    EXPECT_EQ(f.MaterialRow(push, textured).albedoMapID, 0u);
    // the atlas moved into row 0
    EXPECT_EQ(f.MaterialRow(push, glowing).albedoMapID, 1u);
    EXPECT_EQ(f.MaterialRow(push, glowing).emissiveMapID, 0u);
    EXPECT_EQ(f.TextureRow(push, 1).texture, FakeDevice::FirstTextureID + 1);
}

// a frame without textures names no table, and its materials no maps
TEST(TextureTable, NoTexturesNoTable) {
    Fixture f;
    const auto plain = f.AddMaterial({});

    const auto push = f.Frame();
    EXPECT_EQ(push.textures, 0u);
    EXPECT_EQ(push.textureBase, 0u);
    EXPECT_EQ(f.MaterialRow(push, plain).albedoMapID, 0u);
}

// a material written without maps keeps the row's defaults
TEST(TextureTable, RowDefaultsSampleTheWholeMapAndReceiveShadows) {
    const MaterialData row{};
    EXPECT_EQ(row.uvScaleOffset.x, 1.0f);
    EXPECT_EQ(row.uvScaleOffset.y, 1.0f);
    EXPECT_EQ(row.uvScaleOffset.z, 0.0f);
    EXPECT_EQ(row.uvScaleOffset.w, 0.0f);
    EXPECT_EQ(row.alphaCutoff, 0.5f);
    EXPECT_EQ(row.flags, 0u);
}

// Clear drops the extracted rows and keeps the uploads, which only a
// retire through the device frees
TEST(TextureTable, ClearKeepsTexturesAndRetireDefersTheirRelease) {
    Fixture f;
    f.AddTexture(TextureSampler::NearestClamp);
    f.AddTexture(TextureSampler::LinearClamp);
    f.AddMaterial({});

    f.scene.Clear();
    EXPECT_EQ(f.scene.Textures().Count(), 2u);
    EXPECT_TRUE(f.scene.Materials().IsEmpty());

    f.scene.RetireTextures(f.device);
    EXPECT_TRUE(f.scene.Textures().IsEmpty());
    EXPECT_EQ(f.device.deferred.size(), 2u);
    EXPECT_EQ(f.device.texturesDestroyed, 0u);

    f.device.RunDeferred();
    EXPECT_EQ(f.device.texturesDestroyed, 2u);
}
