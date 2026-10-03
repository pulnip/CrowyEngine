#include <stdexcept>
#include <string>

#include <gtest/gtest.h>

#include "JsonLoader.hpp"
#include "StageDocument.hpp"

using namespace Crowy;

namespace
{
    // a scene in the contract's shape, small enough to read: two models
    // (one the box), two materials, two instances, one quad, one light,
    // one camera, two keys
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
    EXPECT_THROW(parse(miniScene("", "3")), std::runtime_error);
}

TEST(StageDocument, OtherVersionThrows) {
    EXPECT_THROW(parse(miniScene("", "2", "2")), std::runtime_error);
}

TEST(StageDocument, PerAxisScaleOnlyOnTheBox) {
    // lamp-a is not the box; its row's sx/sy/sz are overridden by a later key
    EXPECT_THROW(parse(miniScene(R"(, "sy": 2)")), std::runtime_error);
}

TEST(StageDocument, UnknownModelThrows) {
    auto json = miniScene();
    json.replace(json.find(R"("model": "Lamp")"), 15, R"("model": "Lamq")");
    EXPECT_THROW(parse(json), std::runtime_error);
}

TEST(StageDocument, ChannelMissingFromAKeyThrows) {
    EXPECT_THROW(parse(miniScene(R"(, "emissive_channel": "neon")")), std::runtime_error);
}

TEST(StageDocument, UnknownKeyNameThrows) {
    EXPECT_THROW(parse(miniScene(R"(, "keys": ["dusk"])")), std::runtime_error);
}

TEST(StageSprite, ReadsTheSheetAndItsAnimations) {
    const auto sprite = loadJson<StageSprite>(R"({
        "metadata": {"version": 1, "name": "screen", "type": "sprite"},
        "sheet": {"image": "Screens/Sheet.png", "rows": 4, "columns": 2, "frame_size": [768, 432]},
        "animations": [{"name": "loop", "start_row": 0, "start_col": 1, "frame_count": 8, "frame_duration_ms": 160}]
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
