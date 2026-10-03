#include <algorithm>
#include <cmath>
#include <cstring>
#include <numbers>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "EnumUtil.hpp"
#include "JsonLoader.hpp"
#include "StageScene.hpp"

using namespace Crowy;

namespace
{
    f32 radians(f32 degrees) {
        return degrees * std::numbers::pi_v<f32> / 180.0f;
    }

    Vec3 transformPoint(const Mat4& m, Vec3 p) {
        const auto world = m * Vec4{p.x, p.y, p.z, 1.0f};

        return Vec3{world.x, world.y, world.z};
    }

    Vec3 transformDirection(const Mat4& m, Vec3 d) {
        const auto world = m * Vec4{d.x, d.y, d.z, 0.0f};

        return Vec3{world.x, world.y, world.z};
    }

    void expectNear(Vec3 actual, Vec3 expected) {
        constexpr f32 Epsilon = 1e-4f;

        EXPECT_NEAR(actual.x, expected.x, Epsilon);
        EXPECT_NEAR(actual.y, expected.y, Epsilon);
        EXPECT_NEAR(actual.z, expected.z, Epsilon);
    }

    // the models as the loader leaves them: a slot per material and a box of
    // the model's size standing on the ground
    LoadedStage miniStage() {
        // a glowing lamp, a night-keyed box naming a channel nothing glowing
        // uses, a glowing quad, a light off by day, cameras far, near and between
        constexpr CStr MiniScene = R"({
            "metadata": {"version": 1, "name": "Mini", "type": "scene"},
            "models": [
                {"id": "Lamp", "path": "Models/Lamp.fbx", "size": [0.5, 4.0, 0.5], "materials": ["Palette", "PaletteEmissive"]},
                {"id": "BoxGrey", "path": "Models/Box.fbx", "size": [1, 1, 1], "materials": ["Palette"], "box": true}
            ],
            "materials": [
                {"id": "Palette", "kind": "opaque", "texture": "Palette.png", "sampler": "point", "emissive": false, "cutoff": 0, "receives_shadows": true, "fallback": "#808080"},
                {"id": "PaletteEmissive", "kind": "opaque", "texture": "Palette.png", "sampler": "point", "emissive": true, "cutoff": 0, "receives_shadows": true, "fallback": "#FFFFFF"},
                {"id": "Signs", "kind": "opaque", "texture": "Signs.png", "sampler": "linear", "emissive": false, "cutoff": 0, "receives_shadows": false, "fallback": "#000000"}
            ],
            "instances": [
                {"name": "lamp-a", "area": "Street", "model": "Lamp", "x": 1, "y": 0, "z": 2, "yaw": 90, "sx": 1, "sy": 1, "sz": 1},
                {"name": "tower", "area": "NE", "model": "BoxGrey", "x": 0, "y": 0, "z": 0, "yaw": 0, "sx": 2, "sy": 8, "sz": 2, "keys": ["night"], "emissive_channel": "neon"}
            ],
            "quads": [
                {"name": "sign-a", "area": "Street", "material": "Signs", "image": "a", "x": 0, "y": 3, "z": 1, "yaw": 30, "width": 1.5, "height": 0.5, "u0": 0.25, "v0": 0.5, "u1": 0.5, "v1": 0.75, "fallback": "#FF0000", "emissive_channel": "signs"}
            ],
            "lights": [
                {"name": "lamp-a-light", "group": "street_lamps", "kind": "spot", "x": 1, "y": 3.9, "z": 2, "dx": 0, "dy": -1, "dz": 0, "color": "#FFFFFF", "intensity": 60, "range": 16, "inner_angle": 40, "outer_angle": 60, "shadow": 1}
            ],
            "cameras": [
                {"name": "street", "x": -9, "y": 1.6, "z": -9, "yaw": 45, "pitch": 4, "fov": 50, "orthographic": false, "ortho_size": 0},
                {"name": "close", "x": 1, "y": 1, "z": 2.3, "yaw": 180, "pitch": 0, "fov": 50, "orthographic": false, "ortho_size": 0},
                {"name": "between", "x": 1, "y": 1, "z": 3.25, "yaw": 180, "pitch": 0, "fov": 50, "orthographic": false, "ortho_size": 0}
            ],
            "lighting_keys": [
                {"name": "day", "time": 13, "sun_direction": [0, -1, 0], "sun_color": "#FFFFFF", "sun_intensity": 3, "ambient_sky": "#FFFFFF", "ambient_ground": "#000000", "ambient_intensity": 0.5, "sky_zenith": "#0000FF", "sky_horizon": "#FFFFFF", "sky_haze": "#FFFFFF", "emissive_scale": 2, "light_groups": [{"group": "street_lamps", "scale": 0}], "emissive_channels": [{"channel": "fixtures", "scale": 1}, {"channel": "signs", "scale": 0}, {"channel": "neon", "scale": 5}]},
                {"name": "night", "time": 21, "sun_direction": [0, -1, 0], "sun_color": "#8080FF", "sun_intensity": 0.3, "ambient_sky": "#202040", "ambient_ground": "#000000", "ambient_intensity": 0.5, "sky_zenith": "#000010", "sky_horizon": "#000000", "sky_haze": "#000010", "emissive_scale": 2, "light_groups": [{"group": "street_lamps", "scale": 1.2}], "emissive_channels": [{"channel": "fixtures", "scale": 2.6}, {"channel": "signs", "scale": 1}, {"channel": "neon", "scale": 5}]}
            ],
            "default_key": "day",
            "totals": {"models": 2, "materials": 3, "instances": 2, "quads": 1, "lights": 1, "cameras": 3, "lighting_keys": 2}
        })";

        LoadedStage stage{.document = loadJson<StageDocument>(Str(MiniScene))};
        for(const auto& model: stage.document.models) {
            auto& data = stage.models.emplace_back();
            data.bounds = AABB3D{.center = {0.0f, model.size.y * 0.5f, 0.0f}, .halfScale = model.size * 0.5f};
            for(const auto& material: model.materials)
                data.slots.push_back(ModelSlot{.material = material, .bounds = data.bounds});
        }
        for(const auto& material: stage.document.materials)
            stage.samplers.emplace(material.texture, material.sampler);
        stage.unitQuad = makeStageUnitQuad();

        return stage;
    }

    // empty allocations stand in for the pool's
    StageGeometry geometryOf(const LoadedStage& stage) {
        StageGeometry geometry;
        for(const auto& model: stage.models)
            geometry.models.emplace_back(model.slots.size());

        return geometry;
    }

    // empty handles stand in for the uploaded images
    StageTextureHandles texturesOf(const LoadedStage& stage) {
        StageTextureHandles textures;
        for(const auto& material: stage.document.materials)
            textures.emplace(material.texture, TextureHandle{});

        return textures;
    }

    // the check throws, and its message names every one of `words`
    void expectRestart(const LoadedStage& launched, const StageDocument& document, std::vector<Str> words) {
        words.push_back("restart");
        for(const auto& word: words) {
            EXPECT_THAT(
                [&] { checkStageReload(launched, document); },
                testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(word))
            );
        }
    }

    class Fixture {
    public:
        LoadedStage stage = miniStage();
        RenderScene scene;
        StageBindings bindings = populateStage(scene, stage, geometryOf(stage), texturesOf(stage));

        void ApplyKey(usize key) {
            applyStageKey(scene, bindings, stage.document, key);
        }

        bool Visible(PrimitiveHandle primitive) const {
            return hasFlag(scene.Primitives().GetRef(primitive).flags, PrimitiveFlags::Visible);
        }

        Vec3 Emissive(MaterialHandle material) const {
            return scene.Materials().GetRef(material).data.emissive;
        }
    };
}

TEST(StageScene, AnInstanceIsScaledThenYawedThenMoved) {
    const StageInstance row{.position = {1.0f, 0.0f, 2.0f}, .yaw = 90.0f, .scale = {2.0f, 1.0f, 3.0f}};
    const auto toWorld = instanceToWorld(row);

    // a yaw of 90 turns +Z to +X and +X to -Z
    expectNear(transformPoint(toWorld, {1.0f, 0.0f, 0.0f}), {1.0f, 0.0f, 0.0f});
    expectNear(transformPoint(toWorld, {0.0f, 0.0f, 1.0f}), {4.0f, 0.0f, 2.0f});
}

// the contract: the front faces (sin yaw, 0, cos yaw), the right edge lies
// along (-cos yaw, 0, sin yaw), and (u0, v0) is the front's top-left
TEST(StageScene, AQuadsFrontTopLeftTakesItsFirstUV) {
    Fixture f;
    const auto& quad = f.stage.document.quads[0];
    const auto toWorld = quadToWorld(quad);
    const auto yaw = radians(quad.yaw);
    const Vec3 front{std::sin(yaw), 0.0f, std::cos(yaw)};
    const Vec3 right{-std::cos(yaw), 0.0f, std::sin(yaw)};

    ASSERT_FALSE(f.stage.unitQuad.vertices.empty());
    for(const auto& vertex: f.stage.unitQuad.vertices) {
        const auto across = quad.width * (vertex.texCoord.x - 0.5f);
        const auto down = quad.height * (vertex.texCoord.y - 0.5f);
        expectNear(transformPoint(toWorld, vertex.position), quad.position + right * across - unitY() * down);
        expectNear(normalize(transformDirection(toWorld, vertex.normal)), front);
    }

    const auto rect = f.scene.Materials().GetRef(f.bindings.quadMaterials[0]).data.uvScaleOffset;
    EXPECT_FLOAT_EQ(rect.x, 0.25f);
    EXPECT_FLOAT_EQ(rect.y, 0.25f);
    EXPECT_FLOAT_EQ(rect.z, 0.25f);
    EXPECT_FLOAT_EQ(rect.w, 0.5f);
}

TEST(StageScene, AKeyShowsItsRowsAndScalesItsLightGroups) {
    Fixture f;
    const auto& light = f.scene.Lights().GetRef(f.bindings.lights[0]);

    const auto day = applyStageKey(f.scene, f.bindings, f.stage.document, 0);
    EXPECT_FLOAT_EQ(day.x, 1.0f);
    EXPECT_TRUE(f.Visible(f.bindings.instances[0]));
    EXPECT_FALSE(f.Visible(f.bindings.instances[1]));
    EXPECT_TRUE(f.Visible(f.bindings.quads[0]));
    EXPECT_FALSE(light.enabled);
    EXPECT_TRUE(f.scene.Lights().GetRef(f.bindings.sun).enabled);

    f.ApplyKey(1);
    EXPECT_TRUE(f.Visible(f.bindings.instances[1]));
    EXPECT_TRUE(light.enabled);
    EXPECT_FLOAT_EQ(light.intensity, 72.0f);
    EXPECT_NEAR(light.outerConeAngle, radians(60.0f), 1e-6f);
}

// a palette row per channel a glowing model's instance names: the box names
// neon, but nothing it draws glows
TEST(StageScene, EmissiveRowsFollowTheKeysChannels) {
    Fixture f;
    ASSERT_EQ(f.bindings.emissivePalettes.size(), 1u);
    const auto fixtures = f.bindings.emissivePalettes.at("fixtures");
    EXPECT_EQ(f.scene.Materials().Count(), countStageMaterials(f.stage));

    // the lamp's glowing slot draws with its channel's row
    const auto& lamp = f.scene.Primitives().GetRef(f.bindings.instances[0]);
    const auto& materials = f.scene.Meshes().GetRef(lamp.mesh).materials;
    ASSERT_EQ(materials.size(), 2u);
    EXPECT_EQ(materials[0], f.bindings.palette);
    EXPECT_EQ(materials[1], fixtures);

    f.ApplyKey(0);
    EXPECT_FLOAT_EQ(f.Emissive(fixtures).x, 2.0f);
    EXPECT_FLOAT_EQ(f.Emissive(f.bindings.quadMaterials[0]).x, 0.0f);

    f.ApplyKey(1);
    EXPECT_FLOAT_EQ(f.Emissive(fixtures).x, 5.2f);
    EXPECT_FLOAT_EQ(f.Emissive(f.bindings.quadMaterials[0]).x, 2.0f);
}

TEST(StageScene, CutsLookDownAtPositivePitchWithAFittedNearPlane) {
    Fixture f;
    const auto cuts = makeStageCuts(f.stage);
    ASSERT_EQ(cuts.size(), 3u);

    // the scene file's pitch looks up when positive, the engine's down
    EXPECT_NEAR(cuts[0].pitch, -radians(4.0f), 1e-6f);
    EXPECT_NEAR(cuts[0].yaw, radians(45.0f), 1e-6f);
    EXPECT_NEAR(cuts[0].fovY, radians(50.0f), 1e-6f);

    // half the gap to the nearest box, kept within 0.3 and 1 m
    EXPECT_FLOAT_EQ(cuts[0].nearZ, 1.0f);
    EXPECT_FLOAT_EQ(cuts[1].nearZ, 0.3f);
    EXPECT_NEAR(cuts[2].nearZ, 0.5f, 1e-5f);
}

TEST(StageReload, TheSameDocumentPasses) {
    const auto stage = miniStage();

    EXPECT_NO_THROW(checkStageReload(stage, stage.document));
}

// the pool holds only the launch's geometry, the texture table its images
TEST(StageReload, WhatLaunchDidNotLoadNeedsARestart) {
    const auto stage = miniStage();

    auto otherFile = stage.document;
    otherFile.models[0].path = "Models/Other.fbx";
    expectRestart(stage, otherFile, {"Lamp", "Models/Other.fbx"});

    auto added = stage.document;
    added.models.push_back(StageModel{.id = "Bench", .path = "Models/Bench.fbx", .materials = {"Palette"}});
    expectRestart(stage, added, {"Bench"});

    auto otherSlots = stage.document;
    otherSlots.models[0].materials = {"Palette"};
    expectRestart(stage, otherSlots, {"Lamp"});

    auto newImage = stage.document;
    newImage.materials[2].texture = "New.png";
    expectRestart(stage, newImage, {"New.png"});

    // Signs.png was uploaded for a linear sampler
    auto otherSampler = stage.document;
    otherSampler.materials[2].sampler = StageSampler::Point;
    expectRestart(stage, otherSampler, {"Signs.png"});
}

TEST(StageReload, ModelsLeftOutOrReorderedPass) {
    const auto stage = miniStage();
    auto reordered = stage.document;
    std::ranges::reverse(reordered.models);
    EXPECT_NO_THROW(checkStageReload(stage, reordered));

    auto fewer = stage.document;
    fewer.models.pop_back();
    EXPECT_NO_THROW(checkStageReload(stage, fewer));
}

// a reload clears the scene and populates it again: the same file must come
// back in the same slots with the same rows, or no picture would match
TEST(StageScene, AClearedSceneFillsAgainAsBefore) {
    Fixture f;
    f.ApplyKey(1);
    const auto primitives = std::vector(f.scene.Primitives().All().begin(), f.scene.Primitives().All().end());
    const auto materials = std::vector(f.scene.Materials().All().begin(), f.scene.Materials().All().end());
    const auto lights = std::vector(f.scene.Lights().All().begin(), f.scene.Lights().All().end());
    std::vector<usize> meshIndices;
    for(const auto& row: primitives)
        meshIndices.push_back(f.scene.Meshes().IndexOf(row.mesh));
    const auto oldLamp = f.bindings.instances[0];

    f.scene.Clear();
    f.bindings = populateStage(f.scene, f.stage, geometryOf(f.stage), texturesOf(f.stage));
    f.ApplyKey(1);

    EXPECT_FALSE(f.scene.Primitives().IsValid(oldLamp));
    ASSERT_EQ(f.scene.Primitives().Count(), primitives.size());
    ASSERT_EQ(f.scene.Materials().Count(), materials.size());
    ASSERT_EQ(f.scene.Lights().Count(), lights.size());
    for(usize i = 0; i < primitives.size(); ++i) {
        const auto& row = f.scene.Primitives().All()[i];
        EXPECT_EQ(f.scene.Primitives().HandleAt(i).GetIndex(), i);
        EXPECT_EQ(std::memcmp(&row.localToWorld, &primitives[i].localToWorld, sizeof(Mat4)), 0) << i;
        EXPECT_EQ(std::memcmp(&row.worldBounds, &primitives[i].worldBounds, sizeof(AABB3D)), 0) << i;
        EXPECT_EQ(row.flags, primitives[i].flags) << i;
        EXPECT_EQ(f.scene.Meshes().IndexOf(row.mesh), meshIndices[i]) << i;
    }
    for(usize i = 0; i < materials.size(); ++i)
        EXPECT_EQ(std::memcmp(&f.scene.Materials().All()[i].data, &materials[i].data, sizeof(MaterialData)), 0) << i;
    for(usize i = 0; i < lights.size(); ++i) {
        const auto& row = f.scene.Lights().All()[i];
        EXPECT_EQ(row.enabled, lights[i].enabled) << i;
        EXPECT_EQ(row.intensity, lights[i].intensity) << i;
        EXPECT_EQ(row.position, lights[i].position) << i;
    }
}

// a 4 x 2 sheet of 8 frames, 160 ms each, from the top-left cell
TEST(StageScene, AFlipbookMovesItsRectByWholeCells) {
    const StageSprites sprites{{
        "screen.json",
        StageSprite{
            .image = "Screen.png",
            .rows = 4,
            .columns = 2,
            .animations = {
                StageSpriteAnimation{.name = "loop", .frameCount = 8, .frameDurationMs = 160},
                StageSpriteAnimation{.name = "late", .startRow = 1, .startColumn = 1, .frameCount = 3, .frameDurationMs = 100},
            }
        }
    }};
    StageQuad quad{.uv0 = {0.0f, 0.0f}, .uv1 = {0.5f, 0.25f}, .flipbook = StageFlipbook{.sprite = "screen.json", .animation = "loop"}};
    const auto at = [&](f64 seconds) { return stageQuadRect(quad, sprites, seconds); };

    EXPECT_EQ(at(0.0), (Vec4{0.5f, 0.25f, 0.0f, 0.0f}));
    EXPECT_EQ(at(0.159), (Vec4{0.5f, 0.25f, 0.0f, 0.0f}));
    EXPECT_EQ(at(0.16), (Vec4{0.5f, 0.25f, 0.5f, 0.0f}));
    EXPECT_EQ(at(0.32), (Vec4{0.5f, 0.25f, 0.0f, 0.25f}));
    // 0.48 s is a hair under 480 ms in binary; it still lands on frame 3
    EXPECT_EQ(at(0.48), (Vec4{0.5f, 0.25f, 0.5f, 0.25f}));
    EXPECT_EQ(at(1.12), (Vec4{0.5f, 0.25f, 0.5f, 0.75f}));
    EXPECT_EQ(at(1.28), (Vec4{0.5f, 0.25f, 0.0f, 0.0f}));
    EXPECT_EQ(at(-1.0), at(0.0));

    // from row 1, column 1: the second frame wraps to row 2, column 0
    quad.uv0 = {0.5f, 0.25f};
    quad.uv1 = {1.0f, 0.5f};
    quad.flipbook->animation = "late";
    EXPECT_EQ(at(0.1), (Vec4{0.5f, 0.25f, 0.0f, 0.5f}));

    quad.flipbook.reset();
    EXPECT_EQ(at(0.5), (Vec4{0.5f, 0.25f, 0.5f, 0.25f}));
}
