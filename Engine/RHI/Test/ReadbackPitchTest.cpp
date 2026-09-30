#include <gtest/gtest.h>

#include "RHIDefinitions.hpp"

using namespace Crowy;

TEST(ReadbackPitch, ARowRoundsUpToTheBackendsAlignment) {
    constexpr RHICapabilities d3d12{
        .textureRowPitchAlign = 256,
        .textureOffsetAlign = 512
    };
    constexpr RHICapabilities metal{
        .textureRowPitchAlign = 16,
        .textureOffsetAlign = 16
    };

    EXPECT_EQ(
        GetReadbackRowPitch(RHIPixelFormat::RGBA16_FLOAT, 1283, d3d12),
        10496u
    );
    EXPECT_EQ(
        GetReadbackRowPitch(RHIPixelFormat::RGBA16_FLOAT, 1283, metal),
        10272u
    );
    EXPECT_EQ(
        GetReadbackRowPitch(RHIPixelFormat::RGBA8_UNORM, 800, d3d12),
        3328u
    );
    // already aligned rows stay tight
    EXPECT_EQ(
        GetReadbackRowPitch(RHIPixelFormat::D32_FLOAT, 2048, d3d12),
        8192u
    );
    EXPECT_EQ(
        GetReadbackRowPitch(RHIPixelFormat::RGBA16_FLOAT, 1280, d3d12),
        10240u
    );
}
