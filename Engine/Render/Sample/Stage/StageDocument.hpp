#pragma once

#include <filesystem>
#include <format>
#include <optional>
#include <stdexcept>
#include <vector>

#include "DomTraits.hpp"
#include "Primitives.hpp"

// Backlot's scene file (its docs/scene-format.md, metadata version 1), read
// as written: metres and degrees, colors decoded to linear light.
namespace Crowy
{
    struct StageCamera;
    struct StageDocument;
    struct StageInstance;
    struct StageLight;
    struct StageLightingKey;
    struct StageMaterial;
    struct StageModel;
    struct StageQuad;
    struct StageScale;
    struct StageSprite;
    struct StageSpriteAnimation;

    using StageCameras = std::vector<StageCamera>;
    using StageInstances = std::vector<StageInstance>;
    using StageLights = std::vector<StageLight>;
    using StageLightingKeys = std::vector<StageLightingKey>;
    using StageMaterials = std::vector<StageMaterial>;
    using StageModels = std::vector<StageModel>;
    using StageQuads = std::vector<StageQuad>;
    using StageScales = std::vector<StageScale>;
    using StageSpriteAnimations = std::vector<StageSpriteAnimation>;
    using StageNames = std::vector<Str>;

    // the channel an instance's emissive faces glow on when it names none
    inline constexpr CStr DefaultEmissiveChannel = "fixtures";
    inline constexpr CStr StageScenePath = "Data/scene.json";

    // "#RRGGBB", 0..1 per channel and still sRGB-encoded
    inline constexpr Vec3 parseHexColor(StrView text) {
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

    // paths in the file resolve against `root`
    StageDocument loadStageDocument(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    );
    StageDocument loadStageDocument(const std::filesystem::path& root);
    StageSprite loadStageSprite(const StageDocument& document, StrView relative);
    std::filesystem::path resolveStagePath(const StageDocument& document, StrView relative);

    enum class StageMaterialKind : u8 {
        Opaque,
        Masked,
    };

    enum class StageSampler : u8 {
        Point,
        Linear,
    };

    enum class StageLightKind : u8 {
        Point,
        Spot,
    };

    struct StageModel {
        Str id;
        // relative to the content root
        Str path;
        // the asset card's nominal extent, which the model may miss a little
        Vec3 size{};
        StageNames materials;
        // the unit box, the only model scaled per axis
        bool box = false;
    };

    // the palette's surface grain; read, not drawn
    struct StageDetail {
        Str normal;
        Str mask;
        f32 metersPerTile = 1.0f;
        f32 strength = 1.0f;
    };

    struct StageMaterial {
        Str id;
        StageMaterialKind kind = StageMaterialKind::Opaque;
        Str texture;
        StageSampler sampler = StageSampler::Point;
        // the texture is also the emission
        bool emissive = false;
        // alpha below it is cut, for a masked material
        f32 cutoff = 0.0f;
        bool receivesShadows = true;
        // linear
        Vec3 fallback{};
        std::optional<StageDetail> detail;
    };

    struct StageInstance {
        Str name;
        Str area;
        Str model;
        Vec3 position{};
        // degrees, clockwise from +Z seen from above
        f32 yaw = 0.0f;
        Vec3 scale{1.0f, 1.0f, 1.0f};
        Str emissiveChannel;
        // empty: every lighting key
        StageNames keys;
    };

    struct StageFlipbook {
        // the sprite manifest, relative to the content root
        Str sprite;
        Str animation;
    };

    struct StageQuad {
        Str name;
        Str area;
        Str material;
        Str image;
        Vec3 position{};
        // degrees: the direction the image faces, clockwise from +Z
        f32 yaw = 0.0f;
        f32 width = 1.0f;
        f32 height = 1.0f;
        // the image's rect in the atlas, origin at its top-left
        Vec2 uv0{0.0f, 0.0f};
        Vec2 uv1{1.0f, 1.0f};
        Vec3 fallback{};
        // empty: the quad is only lit
        Str emissiveChannel;
        StageNames keys;
        std::optional<StageFlipbook> flipbook;
        // the cut a monitor quad shows
        Str camera;
    };

    struct StageLight {
        Str name;
        Str group;
        StageLightKind kind = StageLightKind::Point;
        Vec3 position{};
        // the way the light travels; zero for a point light
        Vec3 direction{};
        Vec3 color{1.0f, 1.0f, 1.0f};
        // before the lighting key's group scale
        f32 intensity = 0.0f;
        f32 range = 0.0f;
        // cone half-angles, degrees
        f32 innerAngle = 0.0f;
        f32 outerAngle = 0.0f;
        // 0 casts none; higher asks first
        u32 shadowPriority = 0;
    };

    struct StageCamera {
        Str name;
        Vec3 position{};
        // degrees, clockwise from +Z
        f32 yaw = 0.0f;
        // degrees, positive looks up
        f32 pitch = 0.0f;
        // vertical, degrees
        f32 fov = 0.0f;
        bool orthographic = false;
        // half the vertical extent in meters
        f32 orthoSize = 0.0f;
    };

    struct StageScale {
        Str name;
        f32 scale = 0.0f;
    };

    struct StageLightingKey {
        Str name;
        // hour of the day
        f32 time = 0.0f;
        // unit, the way the sunlight travels
        Vec3 sunDirection{0.0f, -1.0f, 0.0f};
        Vec3 sunColor{1.0f, 1.0f, 1.0f};
        f32 sunIntensity = 0.0f;
        bool sunShadows = true;
        Vec3 ambientSky{};
        Vec3 ambientGround{};
        f32 ambientIntensity = 0.0f;
        Vec3 skyZenith{};
        Vec3 skyHorizon{};
        Vec3 skyHaze{};
        f32 emissiveScale = 1.0f;
        StageScales lightGroups;
        StageScales emissiveChannels;
    };

    struct StageDocument {
        u32 version = 0;
        Str name;
        // what every path in the file is relative to
        std::filesystem::path root;
        StageModels models;
        StageMaterials materials;
        StageInstances instances;
        StageQuads quads;
        StageLights lights;
        StageCameras cameras;
        StageLightingKeys lightingKeys;
        Str defaultKey;
    };

    struct StageSpriteAnimation {
        Str name;
        u32 startRow = 0;
        u32 startColumn = 0;
        u32 frameCount = 1;
        u32 frameDurationMs = 0;
    };

    // equal cells, frames running along a row and on to the next
    struct StageSprite {
        // relative to the content root
        Str image;
        u32 rows = 1;
        u32 columns = 1;
        StageSpriteAnimations animations;
    };

    // throws std::runtime_error naming the row and what it got wrong
    template<>
    struct DomTraits<StageDocument> {
        static StageDocument from(const DOM::Value& root, const DocMetadata& metadata);
    };

    template<>
    struct DomTraits<StageSprite> {
        static StageSprite from(const DOM::Value& root, const DocMetadata& metadata);
    };
}
