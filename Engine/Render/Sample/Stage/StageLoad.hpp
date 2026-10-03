#pragma once

#include <filesystem>
#include <map>
#include <vector>

#include "ImageLoader.hpp"
#include "MeshData.hpp"
#include "ModelLoader.hpp"
#include "StageDocument.hpp"

namespace Crowy
{
    struct LoadedStage;

    using StageImages = std::map<Str, ImageData>;
    using StageModelData = std::vector<ModelData>;
    using StageSprites = std::map<Str, StageSprite>;

    // the most the GeometryPool's one upload may stage: a slice of the
    // device's 32 MiB transient ring, with room left for the first frame
    inline constexpr u64 StageStagingBudget = 24ull << 20;

    bool isLfsPointer(const std::filesystem::path& file);

    // everything the scene file names, on the CPU; throws naming a missing
    // file, a Git LFS pointer, or geometry past StageStagingBudget
    LoadedStage loadStage(const std::filesystem::path& root);
    LoadedStage loadStage(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    );

    // the palette, the emissive palette per channel in use, one per quad
    u32 countStageMaterials(const LoadedStage& stage);
    // a unit square facing +Z, u along -X and v down: the contract's corner
    // for (u0, v0) is its front's top-left
    MeshData makeStageUnitQuad();

    // what RenderApp::Config must reserve before the stage is built
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

    struct StageLoadTimings {
        f64 documentSeconds = 0.0;
        f64 modelSeconds = 0.0;
        f64 imageSeconds = 0.0;
    };

    struct LoadedStage {
        StageDocument document;
        // parallel to document.models, kept after upload for picking
        StageModelData models;
        // by the material row's texture path
        StageImages images;
        // by the quad's sprite manifest path
        StageSprites sprites;
        // makeStageUnitQuad's, shared by every quad
        MeshData unitQuad;
        StageCapacities capacities;
        StageLoadTimings timings;
    };
}
