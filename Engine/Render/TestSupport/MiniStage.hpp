#pragma once

#include "JsonLoader.hpp"
#include "StageLoad.hpp"

namespace Crowy
{
    // the models as the loader leaves them: a slot per material and a box of
    // the model's size standing on the ground
    inline LoadedStage miniStage() {
        // a glowing lamp, a night-keyed box naming a channel nothing glowing
        // uses, a glowing quad, a light off by day, cameras far, near and
        // between
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
            data.bounds = AABB3D{
                .center = {0.0f, model.size.y * 0.5f, 0.0f},
                .halfScale = model.size * 0.5f
            };
            for(const auto& material: model.materials)
                data.slots.push_back(
                    ModelSlot{.material = material, .bounds = data.bounds}
                );
        }
        for(const auto& material: stage.document.materials)
            stage.samplers.emplace(material.texture, material.sampler);
        stage.unitQuad = makeStageUnitQuad();
        stage.capacities.materials = countStageMaterials(stage);
        stage.capacities.draws = countStageDraws(stage);

        return stage;
    }
}
