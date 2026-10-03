#include <filesystem>
#include <format>
#include <iostream>

#include <gtest/gtest.h>

#include "StageLoad.hpp"

using namespace Crowy;

namespace
{
    const std::filesystem::path BacklotRoot{CROWY_BACKLOT_DIR};
}

// The whole set on the CPU: what the editor's constructor will do before
// RenderApp exists. Prints the numbers the smoke duration and the pool
// capacities are set from.
TEST(BacklotLoad, TheWholeSetLoadsWithinTheStagingBudget) {
    const auto scene = BacklotRoot / StageScenePath;
    if(!std::filesystem::exists(scene))
        GTEST_SKIP() << std::format("no {} (set CROWY_BACKLOT_DIR)", scene.string());
    if(isLfsPointer(BacklotRoot / "Unity/Assets/Art/Textures/Palette.png"))
        GTEST_SKIP() << "Backlot's files are Git LFS pointers (git lfs pull)";

    const auto stage = loadStage(BacklotRoot);

    EXPECT_EQ(stage.models.size(), 124u);
    // eight atlases and the palette, which both palette materials share
    EXPECT_EQ(stage.images.size(), 9u);
    EXPECT_EQ(stage.sprites.size(), 1u);
    EXPECT_EQ(stage.unitQuad.indices.size(), 6u);

    u64 vertices = 0;
    u64 indices = 0;
    u64 triangles = 0;
    for(const auto& model: stage.models) {
        for(const auto& slot: model.slots) {
            vertices += slot.mesh.vertices.size();
            indices += slot.mesh.indices.size();
            triangles += slot.mesh.indices.size() / 3;
        }
    }
    const auto& capacities = stage.capacities;
    EXPECT_GT(capacities.vertices, vertices);
    EXPECT_GT(capacities.indices, indices);
    EXPECT_LE(capacities.stagingBytes, StageStagingBudget);
    // the palette, an emissive palette per channel in use, one per quad
    EXPECT_GE(capacities.materials, 1u + 1u + 179u);
    EXPECT_GE(capacities.draws, 1488u + 179u);

    const auto& timings = stage.timings;
    std::cout << std::format(
        "stage: {} model vertices, {} indices ({} triangles), staging {:.1f} MiB, "
        "{} materials, {} draws per pass; loaded in {:.2f} s "
        "(document {:.2f}, models {:.2f}, images {:.2f})\n",
        vertices,
        indices,
        triangles,
        static_cast<f64>(capacities.stagingBytes) / (1 << 20),
        capacities.materials,
        capacities.draws,
        timings.documentSeconds + timings.modelSeconds + timings.imageSeconds,
        timings.documentSeconds,
        timings.modelSeconds,
        timings.imageSeconds
    );
}
