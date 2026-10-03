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
    // what RenderApp::Config must reserve before the stage can be built
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

    // Everything the stage needs, on the CPU, before any device work: the
    // document, every placed model (kept after upload, for picking), every
    // texture the materials name, the sprite sheets the quads play, and the
    // one quad every image quad shares.
    struct LoadedStage {
        StageDocument document;
        // parallel to document.models
        std::vector<ModelData> models;
        // by the material row's texture path
        std::map<Str, ImageData> images;
        // by the quad's sprite manifest path
        std::map<Str, StageSprite> sprites;
        // a unit square facing +Z, u along -X and v down, so a quad's
        // (u0, v0) lands on its front's top-left as the contract places it
        MeshData unitQuad;
        StageCapacities capacities;
        StageLoadTimings timings;
    };

    // the most the GeometryPool's one upload may stage: a slice of the
    // device's 32 MiB transient ring, with room left for the first frame
    inline constexpr u64 StageStagingBudget = 24ull << 20;

    // true when `file` holds a Git LFS pointer in place of its content
    bool isLfsPointer(const std::filesystem::path& file);

    // Loads root / Data/scene.json (or `sceneFile`) and everything it names.
    // Throws std::runtime_error naming the file for a missing file, a Git
    // LFS pointer, or geometry past StageStagingBudget.
    LoadedStage loadStage(const std::filesystem::path& root);
    LoadedStage loadStage(
        const std::filesystem::path& root,
        const std::filesystem::path& sceneFile
    );

    // the material rows extraction will add: the palette, the emissive
    // palette once per channel its instances glow on, and one per quad
    u32 countStageMaterials(const LoadedStage& stage);
}
