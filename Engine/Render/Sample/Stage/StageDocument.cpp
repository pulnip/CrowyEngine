#include "StageDocument.hpp"

#include <algorithm>
#include <format>
#include <stdexcept>

#include "ColorSpace.hpp"
#include "JsonLoader.hpp"
#include "StringUtil.hpp"

namespace
{
    using namespace Crowy;

    // where a row sits, for a message that names it
    struct RowRef {
        StrView section;
        usize index = 0;
    };

    [[noreturn]] void refuse(RowRef where, StrView what) {
        throw std::runtime_error(
            std::format("scene file: {}[{}] {}", where.section, where.index, what)
        );
    }

    template<typename T>
    T required(const DOM::Value& row, StrView key, RowRef where) {
        if(auto value = row.get<T>(key))
            return *value;

        refuse(where, std::format("has no readable '{}'", key));
    }

    template<typename T>
    T optional(const DOM::Value& row, StrView key, T fallback) {
        return row.get<T>(key).value_or(std::move(fallback));
    }

    Vec3 requiredColor(const DOM::Value& row, StrView key, RowRef where) {
        return srgbToLinear(parseHexColor(required<Str>(row, key, where)));
    }

    std::vector<Str> strings(const DOM::Value& row, StrView key) {
        std::vector<Str> values;
        row.forEach(key, [&](const DOM::Value& item) {
            if(const auto* text = item.asString())
                values.push_back(*text);
        });

        return values;
    }

    // [{<nameKey>, scale}] rows of a lighting key
    std::vector<StageScale> scales(
        const DOM::Value& row,
        StrView key,
        StrView nameKey,
        RowRef where
    ) {
        std::vector<StageScale> values;
        row.forEach(key, [&](const DOM::Value& item) {
            values.push_back(StageScale{
                .name = required<Str>(item, nameKey, where),
                .scale = required<f32>(item, "scale", where)
            });
        });

        return values;
    }

    // every row of a section, read by `read` with the row's position
    template<typename T, typename F>
    std::vector<T> rows(const DOM::Value& root, StrView section, F&& read) {
        std::vector<T> values;
        root.enumerate(section, [&](isize index, const DOM::Value& row) {
            values.push_back(read(row, RowRef{section, static_cast<usize>(index)}));
        });

        return values;
    }

    Vec3 positionOf(const DOM::Value& row, RowRef where) {
        return Vec3{
            required<f32>(row, "x", where),
            required<f32>(row, "y", where),
            required<f32>(row, "z", where)
        };
    }

    StageModel readModel(const DOM::Value& row, RowRef where) {
        return StageModel{
            .id = required<Str>(row, "id", where),
            .path = required<Str>(row, "path", where),
            .size = required<Vec3>(row, "size", where),
            .materials = strings(row, "materials"),
            .box = optional(row, "box", false)
        };
    }

    StageMaterial readMaterial(const DOM::Value& row, RowRef where) {
        const auto kind = required<Str>(row, "kind", where);
        const auto sampler = required<Str>(row, "sampler", where);
        if(kind != "opaque" && kind != "masked")
            refuse(where, std::format("has kind '{}'", kind));
        if(sampler != "point" && sampler != "linear")
            refuse(where, std::format("has sampler '{}'", sampler));

        StageMaterial material{
            .id = required<Str>(row, "id", where),
            .kind = kind == "masked" ? StageMaterialKind::Masked : StageMaterialKind::Opaque,
            .texture = required<Str>(row, "texture", where),
            .sampler = sampler == "point" ? StageSampler::Point : StageSampler::Linear,
            .emissive = optional(row, "emissive", false),
            .cutoff = optional(row, "cutoff", 0.0f),
            .receivesShadows = optional(row, "receives_shadows", true),
            .fallback = requiredColor(row, "fallback", where)
        };
        if(const auto* detail = row.at("detail")) {
            material.detail = StageDetail{
                .normal = required<Str>(*detail, "normal", where),
                .mask = required<Str>(*detail, "mask", where),
                .metresPerTile = required<f32>(*detail, "metres_per_tile", where),
                .strength = required<f32>(*detail, "strength", where)
            };
        }

        return material;
    }

    StageInstance readInstance(const DOM::Value& row, RowRef where) {
        return StageInstance{
            .name = required<Str>(row, "name", where),
            .area = required<Str>(row, "area", where),
            .model = required<Str>(row, "model", where),
            .position = positionOf(row, where),
            .yaw = required<f32>(row, "yaw", where),
            .scale = Vec3{
                optional(row, "sx", 1.0f),
                optional(row, "sy", 1.0f),
                optional(row, "sz", 1.0f)
            },
            .emissiveChannel = optional(row, "emissive_channel", Str(DefaultEmissiveChannel)),
            .keys = strings(row, "keys")
        };
    }

    StageQuad readQuad(const DOM::Value& row, RowRef where) {
        StageQuad quad{
            .name = required<Str>(row, "name", where),
            .area = required<Str>(row, "area", where),
            .material = required<Str>(row, "material", where),
            .image = optional(row, "image", Str{}),
            .position = positionOf(row, where),
            .yaw = required<f32>(row, "yaw", where),
            .width = required<f32>(row, "width", where),
            .height = required<f32>(row, "height", where),
            .uv0 = Vec2{required<f32>(row, "u0", where), required<f32>(row, "v0", where)},
            .uv1 = Vec2{required<f32>(row, "u1", where), required<f32>(row, "v1", where)},
            .fallback = requiredColor(row, "fallback", where),
            .emissiveChannel = optional(row, "emissive_channel", Str{}),
            .keys = strings(row, "keys"),
            .camera = optional(row, "camera", Str{})
        };
        if(const auto* flipbook = row.at("flipbook")) {
            quad.flipbook = StageFlipbook{
                .sprite = required<Str>(*flipbook, "sprite", where),
                .animation = required<Str>(*flipbook, "animation", where)
            };
        }

        return quad;
    }

    StageLight readLight(const DOM::Value& row, RowRef where) {
        const auto kind = required<Str>(row, "kind", where);
        if(kind != "point" && kind != "spot")
            refuse(where, std::format("has kind '{}'", kind));

        return StageLight{
            .name = required<Str>(row, "name", where),
            .group = required<Str>(row, "group", where),
            .kind = kind == "spot" ? StageLightKind::Spot : StageLightKind::Point,
            .position = positionOf(row, where),
            .direction = Vec3{
                required<f32>(row, "dx", where),
                required<f32>(row, "dy", where),
                required<f32>(row, "dz", where)
            },
            .color = requiredColor(row, "color", where),
            .intensity = required<f32>(row, "intensity", where),
            .range = required<f32>(row, "range", where),
            .innerAngle = optional(row, "inner_angle", 0.0f),
            .outerAngle = optional(row, "outer_angle", 0.0f),
            .shadowPriority = optional(row, "shadow", u32{0})
        };
    }

    StageCamera readCamera(const DOM::Value& row, RowRef where) {
        return StageCamera{
            .name = required<Str>(row, "name", where),
            .position = positionOf(row, where),
            .yaw = required<f32>(row, "yaw", where),
            .pitch = required<f32>(row, "pitch", where),
            .fov = required<f32>(row, "fov", where),
            .orthographic = optional(row, "orthographic", false),
            .orthoSize = optional(row, "ortho_size", 0.0f)
        };
    }

    StageLightingKey readLightingKey(const DOM::Value& row, RowRef where) {
        return StageLightingKey{
            .name = required<Str>(row, "name", where),
            .time = required<f32>(row, "time", where),
            .sunDirection = required<Vec3>(row, "sun_direction", where),
            .sunColor = requiredColor(row, "sun_color", where),
            .sunIntensity = required<f32>(row, "sun_intensity", where),
            .sunShadows = optional(row, "sun_shadows", true),
            .ambientSky = requiredColor(row, "ambient_sky", where),
            .ambientGround = requiredColor(row, "ambient_ground", where),
            .ambientIntensity = required<f32>(row, "ambient_intensity", where),
            .skyZenith = requiredColor(row, "sky_zenith", where),
            .skyHorizon = requiredColor(row, "sky_horizon", where),
            .skyHaze = requiredColor(row, "sky_haze", where),
            .emissiveScale = optional(row, "emissive_scale", 1.0f),
            .lightGroups = scales(row, "light_groups", "group", where),
            .emissiveChannels = scales(row, "emissive_channels", "channel", where)
        };
    }

    void checkTotal(const DOM::Value& root, StrView section, usize count) {
        const auto total = root.get<usize>(std::format("totals.{}", section));
        if(total && *total != count) {
            throw std::runtime_error(std::format(
                "scene file: '{}' has {} rows, totals says {}", section, count, *total
            ));
        }
    }

    template<typename T, typename Key>
    bool contains(const std::vector<T>& rows, StrView value, Key key) {
        return std::ranges::any_of(rows, [&](const T& row) { return row.*key == value; });
    }

    bool namesScale(const std::vector<StageScale>& scales, StrView name) {
        return contains(scales, name, &StageScale::name);
    }

    void checkKeys(const std::vector<Str>& keys, const StageDocument& document, RowRef where) {
        for(const auto& key: keys) {
            if(!contains(document.lightingKeys, key, &StageLightingKey::name))
                refuse(where, std::format("shows in unknown key '{}'", key));
        }
    }

    // every reference resolves, and every key scales every channel and
    // group a row uses
    void validate(const StageDocument& document) {
        for(usize i = 0; i < document.models.size(); ++i) {
            for(const auto& material: document.models[i].materials) {
                if(!contains(document.materials, material, &StageMaterial::id))
                    refuse(RowRef{"models", i}, std::format("names unknown material '{}'", material));
            }
        }

        for(usize i = 0; i < document.instances.size(); ++i) {
            const RowRef where{"instances", i};
            const auto& instance = document.instances[i];
            const auto model = std::ranges::find(document.models, instance.model, &StageModel::id);
            if(model == document.models.end())
                refuse(where, std::format("places unknown model '{}'", instance.model));

            const auto& s = instance.scale;
            if(!model->box && (s.x != s.y || s.y != s.z))
                refuse(where, "scales a model other than the box per axis");
            checkKeys(instance.keys, document, where);
            for(const auto& key: document.lightingKeys) {
                if(!namesScale(key.emissiveChannels, instance.emissiveChannel))
                    refuse(where, std::format("glows on '{}', which key '{}' leaves out", instance.emissiveChannel, key.name));
            }
        }

        for(usize i = 0; i < document.quads.size(); ++i) {
            const RowRef where{"quads", i};
            const auto& quad = document.quads[i];
            if(!contains(document.materials, quad.material, &StageMaterial::id))
                refuse(where, std::format("uses unknown material '{}'", quad.material));
            if(!quad.camera.empty() && !contains(document.cameras, quad.camera, &StageCamera::name))
                refuse(where, std::format("shows unknown cut '{}'", quad.camera));
            checkKeys(quad.keys, document, where);
            if(quad.emissiveChannel.empty())
                continue;
            for(const auto& key: document.lightingKeys) {
                if(!namesScale(key.emissiveChannels, quad.emissiveChannel))
                    refuse(where, std::format("glows on '{}', which key '{}' leaves out", quad.emissiveChannel, key.name));
            }
        }

        for(usize i = 0; i < document.lights.size(); ++i) {
            const auto& light = document.lights[i];
            for(const auto& key: document.lightingKeys) {
                if(!namesScale(key.lightGroups, light.group))
                    refuse(RowRef{"lights", i}, std::format("is in group '{}', which key '{}' leaves out", light.group, key.name));
            }
        }

        if(!contains(document.lightingKeys, document.defaultKey, &StageLightingKey::name)) {
            throw std::runtime_error(std::format(
                "scene file: default_key '{}' names no lighting key", document.defaultKey
            ));
        }
    }
}

namespace Crowy
{
    StageDocument DomTraits<StageDocument>::from(
        const DOM::Value& root,
        const DocMetadata& metadata
    ) {
        if(metadata.type != "scene") {
            throw std::runtime_error(std::format(
                "expected a \"scene\" document, got \"{}\"", metadata.type
            ));
        }
        if(metadata.version != 1) {
            throw std::runtime_error(std::format(
                "scene file version {} is not the 1 this reader knows", metadata.version
            ));
        }

        StageDocument document{
            .version = metadata.version,
            .name = metadata.name,
            .models = rows<StageModel>(root, "models", readModel),
            .materials = rows<StageMaterial>(root, "materials", readMaterial),
            .instances = rows<StageInstance>(root, "instances", readInstance),
            .quads = rows<StageQuad>(root, "quads", readQuad),
            .lights = rows<StageLight>(root, "lights", readLight),
            .cameras = rows<StageCamera>(root, "cameras", readCamera),
            .lightingKeys = rows<StageLightingKey>(root, "lighting_keys", readLightingKey),
            .defaultKey = root.get<Str>("default_key").value_or(Str{})
        };

        checkTotal(root, "models", document.models.size());
        checkTotal(root, "materials", document.materials.size());
        checkTotal(root, "instances", document.instances.size());
        checkTotal(root, "quads", document.quads.size());
        checkTotal(root, "lights", document.lights.size());
        checkTotal(root, "cameras", document.cameras.size());
        checkTotal(root, "lighting_keys", document.lightingKeys.size());
        validate(document);

        return document;
    }

    StageSprite DomTraits<StageSprite>::from(
        const DOM::Value& root,
        const DocMetadata& metadata
    ) {
        if(metadata.type != "sprite") {
            throw std::runtime_error(std::format(
                "expected a \"sprite\" document, got \"{}\"", metadata.type
            ));
        }

        const RowRef sheet{"sheet", 0};
        StageSprite sprite{
            .image = required<Str>(root, "sheet.image", sheet),
            .rows = required<u32>(root, "sheet.rows", sheet),
            .columns = required<u32>(root, "sheet.columns", sheet),
            .animations = rows<StageSpriteAnimation>(
                root,
                "animations",
                [](const DOM::Value& row, RowRef where) {
                    return StageSpriteAnimation{
                        .name = required<Str>(row, "name", where),
                        .startRow = optional(row, "start_row", u32{0}),
                        .startColumn = optional(row, "start_col", u32{0}),
                        .frameCount = required<u32>(row, "frame_count", where),
                        .frameDurationMs = required<u32>(row, "frame_duration_ms", where)
                    };
                }
            )
        };
        if(sprite.rows == 0 || sprite.columns == 0)
            refuse(sheet, "has an empty grid");

        return sprite;
    }

    Vec3 parseHexColor(StrView text) {
        const auto malformed = [text] {
            return std::runtime_error(std::format("'{}' is not a #RRGGBB color", text));
        };
        if(text.size() != 7 || text[0] != '#')
            throw malformed();

        const auto digit = [&](usize i) -> u32 {
            const auto c = text[i];
            if(c >= '0' && c <= '9')
                return static_cast<u32>(c - '0');
            if(c >= 'a' && c <= 'f')
                return static_cast<u32>(c - 'a' + 10);
            if(c >= 'A' && c <= 'F')
                return static_cast<u32>(c - 'A' + 10);

            throw malformed();
        };
        const auto channel = [&](usize i) {
            return static_cast<f32>(digit(i) * 16 + digit(i + 1)) / 255.0f;
        };

        return Vec3{channel(1), channel(3), channel(5)};
    }

    StageDocument loadStageDocument(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    ) {
        auto document = loadJsonFile<StageDocument>(sceneFile);
        document.root = root;

        return document;
    }

    StageDocument loadStageDocument(const std::filesystem::path& root) {
        return loadStageDocument(root, root / StageScenePath);
    }

    StageSprite loadStageSprite(const StageDocument& document, StrView relative) {
        return loadJsonFile<StageSprite>(resolveStagePath(document, relative));
    }

    std::filesystem::path resolveStagePath(const StageDocument& document, StrView relative) {
        return document.root / toPath(Str(relative).c_str());
    }
}
