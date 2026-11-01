#include <gtest/gtest.h>

#include "EditorCamera.hpp"
#include "LinearAlgebra.hpp"

using namespace Crowy;

TEST(EditorCut, ALookTurnedFromACameraSnapshotComesBackAsTheSameCut) {
    const EditorCut yawed{
        .name = "scene",
        .position = {1.0f, 2.0f, -3.0f},
        .yaw = 2.5f,
        .pitch = -0.4f,
        .lens = {.fovY = 0.9f, .nearZ = 0.2f, .farZ = 80.0f}
    };
    const EditorCamera camera{yawed};

    const auto cut = editorCutOf(
        CameraSnapshot{
            .view = camera.View(),
            .position = camera.Position(),
            .fovY = yawed.lens.fovY,
            .nearZ = yawed.lens.nearZ,
            .farZ = yawed.lens.farZ
        }
    );

    EXPECT_EQ(cut.name, "scene");
    EXPECT_EQ(cut.position, yawed.position);
    EXPECT_NEAR(cut.yaw, yawed.yaw, 1e-5f);
    EXPECT_NEAR(cut.pitch, yawed.pitch, 1e-5f);
    EXPECT_EQ(cut.lens.fovY, yawed.lens.fovY);
    EXPECT_EQ(cut.lens.nearZ, yawed.lens.nearZ);
    EXPECT_EQ(cut.lens.farZ, yawed.lens.farZ);
}
