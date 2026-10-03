#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include "DomTraits.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    // Backlot's scene file (its docs/scene-format.md, metadata version 1),
    // read faithfully: positions in metres and angles in degrees as the
    // file writes them, so an inspector shows the contract's numbers;
    // colors decoded to linear light. Extraction gives the rows meaning.

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
        // extent in metres, in the engine's frame; the asset card's nominal
        // size, which the model may miss by the card's tolerance
        Vec3 size{};
        std::vector<Str> materials;
        // the unit box, the only model scaled per axis
        bool box = false;
    };

    // the palette's surface grain; read, not drawn
    struct StageDetail {
        Str normal;
        Str mask;
        f32 metresPerTile = 1.0f;
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
        // linear; what draws where the texture cannot be sampled
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
        // the channel its emissive faces glow on
        Str emissiveChannel;
        // the lighting keys it shows in; empty means every key
        std::vector<Str> keys;
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
        // the image's name in its atlas manifest, for reference
        Str image;
        // the rectangle's centre
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
        std::vector<Str> keys;
        std::optional<StageFlipbook> flipbook;
        // the cut a monitor quad shows; empty for any other quad
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
        // half the vertical extent in metres
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
        std::vector<StageScale> lightGroups;
        std::vector<StageScale> emissiveChannels;
    };

    struct StageDocument {
        u32 version = 0;
        Str name;
        // what every path in the file is relative to
        std::filesystem::path root;
        std::vector<StageModel> models;
        std::vector<StageMaterial> materials;
        std::vector<StageInstance> instances;
        std::vector<StageQuad> quads;
        std::vector<StageLight> lights;
        std::vector<StageCamera> cameras;
        std::vector<StageLightingKey> lightingKeys;
        Str defaultKey;
    };

    struct StageSpriteAnimation {
        Str name;
        u32 startRow = 0;
        u32 startColumn = 0;
        u32 frameCount = 1;
        u32 frameDurationMs = 0;
    };

    // a sprite sheet manifest (metadata type "sprite"): a grid of equal
    // cells, frames running along a row and on to the next
    struct StageSprite {
        // relative to the content root
        Str image;
        u32 rows = 1;
        u32 columns = 1;
        std::vector<StageSpriteAnimation> animations;
    };

    // the channel an instance's emissive faces glow on when it names none
    inline constexpr CStr DefaultEmissiveChannel = "fixtures";
    // the scene file under a content root
    inline constexpr CStr StageScenePath = "Data/scene.json";

    template<>
    struct DomTraits<StageDocument> {
        // Throws std::runtime_error naming the row and the key it could not
        // read, a reference that does not resolve, a per-axis scale on a
        // model that is not the box, a channel or group a key leaves out,
        // or a section whose length disagrees with `totals`. Keys it does
        // not know are ignored, as the contract allows.
        static StageDocument from(const DOM::Value& root, const DocMetadata& metadata);
    };

    template<>
    struct DomTraits<StageSprite> {
        static StageSprite from(const DOM::Value& root, const DocMetadata& metadata);
    };

    // "#RRGGBB" as written, 0..1 per channel and still sRGB-encoded;
    // throws on any other form
    Vec3 parseHexColor(StrView text);

    // reads `sceneFile`; paths in it resolve against `root`
    StageDocument loadStageDocument(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    );
    // reads root / StageScenePath
    StageDocument loadStageDocument(const std::filesystem::path& root);

    StageSprite loadStageSprite(const StageDocument& document, StrView relative);

    // what an absolute path to one of the document's files is
    std::filesystem::path resolveStagePath(const StageDocument& document, StrView relative);
}
