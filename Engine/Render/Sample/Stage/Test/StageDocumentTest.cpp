#include <stdexcept>
#include <string>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "JsonLoader.hpp"
#include "StageDocument.hpp"

using namespace Crowy;

namespace
{
    // the contract's shape at its smallest: two models (one the box), three
    // materials, two instances, a quad, a light, a camera, two keys
    std::string miniScene(
        std::string_view instanceExtra = "",
        std::string_view totalsInstances = "2",
        std::string_view version = "1"
    ) {
        return std::string(R"({"metadata": {"version": )") + std::string(version)
            + R"(, "name": "Mini", "type": "scene"},
            "models": [
                {"id": "Lamp", "path": "Models/Lamp.fbx", "size": [0.5, 4.0, 0.5], "materials": ["Palette", "PaletteEmissive"]},
                {"id": "BoxGrey", "path": "Models/Box.fbx", "size": [1, 1, 1], "materials": ["Palette"], "box": true}
            ],
            "materials": [
                {"id": "Palette", "kind": "opaque", "texture": "Palette.png", "sampler": "point", "emissive": false, "cutoff": 0, "receives_shadows": true, "fallback": "#808080"},
                {"id": "PaletteEmissive", "kind": "opaque", "texture": "Palette.png", "sampler": "point", "emissive": true, "cutoff": 0, "receives_shadows": true, "fallback": "#FFFFFF"},
                {"id": "Signs", "kind": "masked", "texture": "Signs.png", "sampler": "linear", "emissive": false, "cutoff": 0.5, "receives_shadows": false, "fallback": "#000000", "brand_new_key": 3}
            ],
            "instances": [
                {"name": "lamp-a", "area": "Street", "model": "Lamp", "x": 1, "y": 0, "z": 2, "yaw": 90, "sx": 1, "sy": 1, "sz": 1)"
            + std::string(instanceExtra) + R"(},
                {"name": "tower", "area": "NE", "model": "BoxGrey", "x": 0, "y": 0, "z": 0, "yaw": 0, "sx": 2, "sy": 8, "sz": 2, "keys": ["night"]}
            ],
            "quads": [
                {"name": "sign-a", "area": "Street", "material": "Signs", "image": "a", "x": 0, "y": 3, "z": 1, "yaw": 180, "width": 1.5, "height": 0.5, "u0": 0.25, "v0": 0.5, "u1": 0.5, "v1": 0.75, "fallback": "#FF0000", "emissive_channel": "signs"}
            ],
            "lights": [
                {"name": "lamp-a-light", "group": "street_lamps", "kind": "spot", "x": 1, "y": 3.9, "z": 2, "dx": 0, "dy": -1, "dz": 0, "color": "#FFFFFF", "intensity": 60, "range": 16, "inner_angle": 40, "outer_angle": 60, "shadow": 1}
            ],
            "cameras": [
                {"name": "street", "x": -9, "y": 1.6, "z": -9, "yaw": 45, "pitch": 4, "fov": 50, "orthographic": false, "ortho_size": 0}
            ],
            "lighting_keys": [
                {"name": "day", "time": 13, "sun_direction": [0, -1, 0], "sun_color": "#FFFFFF", "sun_intensity": 3, "ambient_sky": "#FFFFFF", "ambient_ground": "#000000", "ambient_intensity": 0.5, "sky_zenith": "#0000FF", "sky_horizon": "#FFFFFF", "sky_haze": "#FFFFFF", "light_groups": [{"group": "street_lamps", "scale": 0}], "emissive_channels": [{"channel": "fixtures", "scale": 1}, {"channel": "signs", "scale": 0}]},
                {"name": "night", "time": 21, "sun_direction": [0, -1, 0], "sun_color": "#8080FF", "sun_intensity": 0.3, "ambient_sky": "#202040", "ambient_ground": "#000000", "ambient_intensity": 0.5, "sky_zenith": "#000010", "sky_horizon": "#202040", "sky_haze": "#000010", "light_groups": [{"group": "street_lamps", "scale": 1.2}], "emissive_channels": [{"channel": "fixtures", "scale": 2.6}, {"channel": "signs", "scale": 1}]}
            ],
            "default_key": "day",
            "totals": {"models": 2, "materials": 3, "instances": )"
            + std::string(totalsInstances)
            + R"(, "quads": 1, "lights": 1, "cameras": 1, "lighting_keys": 2}
        })";
    }

    StageDocument parse(const std::string& json) {
        return loadJson<StageDocument>(json);
    }

    // the mini scene with its first `from` replaced by `to`
    std::string edited(StrView from, StrView to) {
        auto json = miniScene();
        const auto at = json.find(from);
        EXPECT_NE(at, std::string::npos) << from;
        json.replace(at, from.size(), to);

        return json;
    }

    // the parse throws, and its message names `reason`
    void expectRefusal(const std::string& json, StrView reason) {
        EXPECT_THAT(
            [&] { parse(json); },
            testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(Str(reason)))
        );
    }
}

TEST(StageColor, HexIsReadPerChannel) {
    const auto color = parseHexColor("#FF8000");
    EXPECT_FLOAT_EQ(color.x, 1.0f);
    EXPECT_NEAR(color.y, 128.0f / 255.0f, 1e-6f);
    EXPECT_FLOAT_EQ(color.z, 0.0f);
    EXPECT_FLOAT_EQ(parseHexColor("#1fa6a0").y, parseHexColor("#1FA6A0").y);
}

TEST(StageColor, MalformedHexThrows) {
    EXPECT_THROW(parseHexColor("FF8000"), std::runtime_error);
    EXPECT_THROW(parseHexColor("#FF80"), std::runtime_error);
    EXPECT_THROW(parseHexColor("#GG8000"), std::runtime_error);
}

TEST(StageDocument, ReadsTheFileAsWritten) {
    const auto stage = parse(miniScene());

    ASSERT_EQ(stage.instances.size(), 2u);
    const auto& lamp = stage.instances[0];
    EXPECT_FLOAT_EQ(lamp.yaw, 90.0f);
    EXPECT_EQ(lamp.emissiveChannel, DefaultEmissiveChannel);
    EXPECT_TRUE(lamp.keys.empty());
    EXPECT_EQ(stage.instances[1].keys, std::vector<Str>{"night"});

    // #808080 decoded to linear
    EXPECT_NEAR(stage.materials[0].fallback.x, 0.2158605f, 1e-6f);
    EXPECT_EQ(stage.materials[2].kind, StageMaterialKind::Masked);
    EXPECT_EQ(stage.materials[2].sampler, StageSampler::Linear);
    EXPECT_FALSE(stage.materials[2].receivesShadows);

    const auto& sign = stage.quads[0];
    EXPECT_FLOAT_EQ(sign.uv0.x, 0.25f);
    EXPECT_FLOAT_EQ(sign.uv1.y, 0.75f);
    EXPECT_EQ(sign.emissiveChannel, "signs");

    const auto& light = stage.lights[0];
    EXPECT_EQ(light.kind, StageLightKind::Spot);
    EXPECT_FLOAT_EQ(light.innerAngle, 40.0f);
    EXPECT_EQ(light.shadowPriority, 1u);

    EXPECT_FLOAT_EQ(stage.cameras[0].pitch, 4.0f);
    EXPECT_EQ(stage.lightingKeys[1].lightGroups[0].name, "street_lamps");
    EXPECT_FLOAT_EQ(stage.lightingKeys[1].lightGroups[0].scale, 1.2f);
    EXPECT_EQ(stage.defaultKey, "day");
}

TEST(StageDocument, TotalsMismatchThrows) {
    expectRefusal(miniScene("", "3"), "totals says 3");
}

TEST(StageDocument, MissingTotalsThrows) {
    expectRefusal(edited(R"("totals")", R"("totalz")"), "totals.models");
}

TEST(StageDocument, OtherVersionThrows) {
    expectRefusal(miniScene("", "2", "2"), "version 2");
}

TEST(StageDocument, OtherTypeThrows) {
    expectRefusal(edited(R"("type": "scene")", R"("type": "app")"), "\"app\"");
}

TEST(StageDocument, PerAxisScaleOnlyOnTheBox) {
    // the later sy wins over the row's own
    expectRefusal(miniScene(R"(, "sy": 2)"), "per axis");
}

TEST(StageDocument, UnknownModelThrows) {
    expectRefusal(edited(R"("model": "Lamp")", R"("model": "Lamq")"), "unknown model 'Lamq'");
}

TEST(StageDocument, ModelWithUnknownMaterialThrows) {
    expectRefusal(edited(R"("materials": ["Palette"], "box")", R"("materials": ["Paint"], "box")"), "unknown material 'Paint'");
}

TEST(StageDocument, ModelWithoutMaterialsThrows) {
    expectRefusal(edited(R"("materials": ["Palette"], "box")", R"("materials": [], "box")"), "names no material");
}

TEST(StageDocument, QuadWithUnknownMaterialThrows) {
    expectRefusal(edited(R"("material": "Signs")", R"("material": "Sings")"), "unknown material 'Sings'");
}

TEST(StageDocument, QuadShowingUnknownCutThrows) {
    expectRefusal(edited(R"("emissive_channel": "signs"})", R"("emissive_channel": "signs", "camera": "cameraZ"})"), "unknown cut 'cameraZ'");
}

TEST(StageDocument, InstanceChannelMissingFromAKeyThrows) {
    expectRefusal(miniScene(R"(, "emissive_channel": "neon")"), "glows on 'neon'");
}

TEST(StageDocument, QuadChannelMissingFromAKeyThrows) {
    expectRefusal(edited(R"("emissive_channel": "signs"})", R"("emissive_channel": "game"})"), "glows on 'game'");
}

TEST(StageDocument, LightGroupMissingFromAKeyThrows) {
    expectRefusal(edited(R"("group": "street_lamps", "kind")", R"("group": "set_lights", "kind")"), "group 'set_lights'");
}

TEST(StageDocument, UnknownKeyNameThrows) {
    expectRefusal(miniScene(R"(, "keys": ["dusk"])"), "unknown key 'dusk'");
}

TEST(StageDocument, EmptyKeyListThrows) {
    expectRefusal(miniScene(R"(, "keys": [])"), "shows in no key");
}

TEST(StageDocument, KeysThatAreNotAListThrow) {
    expectRefusal(miniScene(R"(, "keys": "night")"), "not a list");
}

TEST(StageDocument, UnknownDefaultKeyThrows) {
    expectRefusal(edited(R"("default_key": "day")", R"("default_key": "noon")"), "default_key 'noon'");
}

TEST(StageDocument, ATextureWithTwoSamplersThrows) {
    expectRefusal(edited(R"("texture": "Signs.png", "sampler": "linear")", R"("texture": "Palette.png", "sampler": "linear")"), "unlike");
}

TEST(StageDocument, BadKindOrSamplerThrows) {
    expectRefusal(edited(R"("kind": "masked")", R"("kind": "cutout")"), "kind 'cutout'");
    expectRefusal(edited(R"("sampler": "linear")", R"("sampler": "cubic")"), "sampler 'cubic'");
    expectRefusal(edited(R"("kind": "spot")", R"("kind": "area")"), "kind 'area'");
}

// a key that is present must read; only an absent one takes the default
TEST(StageDocument, OptionalKeyOfTheWrongTypeThrows) {
    expectRefusal(edited(R"("shadow": 1})", R"("shadow": 1.5})"), "'shadow'");
    expectRefusal(edited(R"("receives_shadows": false)", R"("receives_shadows": 0)"), "'receives_shadows'");
}

TEST(StageSprite, ReadsTheSheetAndItsAnimations) {
    const auto sprite = loadJson<StageSprite>(R"({
        "metadata": {"version": 1, "name": "screen", "type": "sprite"},
        "sheet": {"image": "Screens/Sheet.png", "rows": 4, "columns": 2, "frame_size": [768, 432]},
        "animations": [{"name": "loop", "start_row": 0, "start_col": 1, "frame_count": 7, "frame_duration_ms": 160}]
    })");
    EXPECT_EQ(sprite.image, "Screens/Sheet.png");
    EXPECT_EQ(sprite.rows, 4u);
    EXPECT_EQ(sprite.columns, 2u);
    ASSERT_EQ(sprite.animations.size(), 1u);
    EXPECT_EQ(sprite.animations[0].startColumn, 1u);
    EXPECT_EQ(sprite.animations[0].frameDurationMs, 160u);
}

TEST(StageSprite, WrongTypeThrows) {
    EXPECT_THROW(
        loadJson<StageSprite>(R"({"metadata": {"version": 1, "name": "x", "type": "scene"}, "sheet": {"image": "a", "rows": 1, "columns": 1}})"),
        std::runtime_error
    );
}

TEST(StageSprite, EmptyGridThrows) {
    EXPECT_THAT(
        [] {
            loadJson<StageSprite>(R"({"metadata": {"version": 1, "name": "x", "type": "sprite"}, "sheet": {"image": "a", "rows": 0, "columns": 2}, "animations": []})");
        },
        testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr("empty grid"))
    );
}

TEST(StageSprite, AnAnimationMustPlayInsideItsSheet) {
    const auto sprite = [](std::string animation) {
        return std::string(R"({"metadata": {"version": 1, "name": "x", "type": "sprite"}, "sheet": {"image": "a", "rows": 2, "columns": 2}, "animations": [)")
            + animation + "]}";
    };
    const auto refused = [&](std::string animation, StrView reason) {
        EXPECT_THAT(
            [&] { loadJson<StageSprite>(sprite(animation)); },
            testing::ThrowsMessage<std::runtime_error>(testing::HasSubstr(Str(reason)))
        );
    };

    refused(R"({"name": "still", "frame_count": 0, "frame_duration_ms": 100})", "plays no frames");
    refused(R"({"name": "frozen", "frame_count": 2, "frame_duration_ms": 0})", "plays no frames");
    refused(R"({"name": "long", "start_row": 1, "frame_count": 3, "frame_duration_ms": 100})", "runs past");
    refused(R"({"name": "wide", "start_col": 2, "frame_count": 1, "frame_duration_ms": 100})", "runs past");
    EXPECT_NO_THROW(loadJson<StageSprite>(sprite(R"({"name": "all", "frame_count": 4, "frame_duration_ms": 100})")));
}
