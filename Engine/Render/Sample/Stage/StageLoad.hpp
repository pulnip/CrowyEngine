#pragma once

#include <filesystem>
#include <map>
#include <optional>
#include <set>
#include <vector>

#include "ImageLoader.hpp"
#include "MeshData.hpp"
#include "ModelLoader.hpp"
#include "StageDocument.hpp"

namespace Crowy
{
    struct LoadedStage;
    struct StageMaterialPlan;
    struct StageReload;

    using StageImages = std::map<Str, ImageData>;
    using StageModelData = std::vector<ModelData>;
    using StageSamplers = std::map<Str, StageSampler>;
    using StageSprites = std::map<Str, StageSprite>;

    bool isLfsPointer(const std::filesystem::path& file);
    // the file that keeps the checkout at `root` from loading at all: no scene
    // file, or a palette still a Git LFS pointer
    std::optional<std::filesystem::path> missingStageContent(
        const std::filesystem::path& root
    );

    // everything the scene file names, on the CPU; throws std::runtime_error
    // naming the file or row it cannot use
    LoadedStage loadStage(const std::filesystem::path& root);
    LoadedStage loadStage(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    );

    // every flipbook's sprite manifest, each path read once
    StageSprites loadStageSprites(const StageDocument& document);
    // throws std::runtime_error naming a model or a texture `document` needs
    // that `launched` did not load, which only a restart loads
    void checkStageReload(
        const LoadedStage& launched,
        const StageDocument& document
    );
    // the scene file read again beside what `launched` holds: its paths under
    // the launch's root, its model table the launch's, which the geometry
    // follows; throws as loading and checkStageReload do
    StageReload reloadStageDocument(
        const LoadedStage& launched,
        const std::filesystem::path& sceneFile
    );

    // the palette, the emissive palette per channel in use, one per quad
    // throws std::runtime_error when a model draws with a material the file
    // does not hold, or with none that does not glow
    StageMaterialPlan planStageMaterials(
        const StageDocument& document,
        const StageModels& table,
        const StageModelData& models
    );
    u32 countStageMaterials(const LoadedStage& stage);
    u32 countStageDraws(const LoadedStage& stage);
    // a unit square facing +Z, u along -X and v down: the contract's corner
    // for (u0, v0) is its front's top-left
    MeshData makeStageUnitQuad();

    // the palette, then an emissive palette per channel
    struct StageMaterialPlan {
        const StageMaterial* palette = nullptr;
        // parallel to the model table
        std::vector<bool> glowingModels;
        // the channels a glowing model's instances name
        std::set<Str> channels;
    };

    // what the renderer must reserve before the stage is built
    struct StageCapacities {
        // GeometryPool elements, with the slack its allocator needs
        u32 vertices = 0;
        u32 indices = 0;
        u32 materials = 0;
        // one per drawn submesh, every keyed row counted
        u32 draws = 0;
        // what the pool's one upload stages in the transient ring
        u64 stagingBytes = 0;
    };

    struct LoadedStage {
        StageDocument document;
        // parallel to document.models, kept after upload for picking
        StageModelData models;
        // by the material row's texture path
        StageImages images;
        // by texture path, the sampler each image was uploaded for
        StageSamplers samplers;
        // by the quad's sprite manifest path
        StageSprites sprites;
        // makeStageUnitQuad's, shared by every quad
        MeshData unitQuad;
        StageCapacities capacities;
    };

    // what a reload replaces; the rest of LoadedStage stays
    struct StageReload {
        StageDocument document;
        StageSprites sprites;
    };
}
