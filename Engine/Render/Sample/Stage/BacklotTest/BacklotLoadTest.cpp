#include <chrono>
#include <filesystem>
#include <format>
#include <iostream>

#include <gtest/gtest.h>

#include "BacklotContent.hpp"
#include "GeometryPool.hpp"
#include "StageLoad.hpp"

using namespace Crowy;

// prints the numbers the smoke window and the pool capacities come from
TEST(BacklotLoad, TheWholeSetLoadsWithinTheStagingBudget) {
    const auto start = std::chrono::steady_clock::now();
    const auto stage = loadStage(backlotRoot());
    const std::chrono::duration<f64> seconds =
        std::chrono::steady_clock::now() - start;

    EXPECT_EQ(stage.models.size(), stage.document.models.size());
    // eight atlases and the palette, which both palette materials share
    EXPECT_EQ(stage.images.size(), 9u);
    EXPECT_EQ(stage.sprites.size(), 1u);
    EXPECT_EQ(stage.unitQuad.indices.size(), 6u);
    // linear-sampled atlases carry mips, the point-sampled ones one level
    EXPECT_EQ(
        stage.images.at("Unity/Assets/Art/Textures/Posters/PosterAtlas.png")
            .mipLevels,
        12u
    );
    EXPECT_EQ(
        stage.images.at("Unity/Assets/Art/Textures/Signs/SignAtlas.png")
            .mipLevels,
        1u
    );
    EXPECT_EQ(
        stage.images.at("Unity/Assets/Art/Textures/Palette.png").mipLevels,
        1u
    );

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
    EXPECT_LE(capacities.stagingBytes, GeometryStagingBudget);
    EXPECT_GE(capacities.materials, 1u + 1u + BacklotQuads);
    EXPECT_GE(capacities.draws, BacklotInstances + BacklotQuads);

    std::cout << std::format(
        "stage: {} model vertices, {} indices ({} triangles), staging {:.1f} "
        "MiB, {} materials, {} draws per pass; loaded in {:.2f} s\n",
        vertices,
        indices,
        triangles,
        static_cast<f64>(capacities.stagingBytes) / (1 << 20),
        capacities.materials,
        capacities.draws,
        seconds.count()
    );
}
