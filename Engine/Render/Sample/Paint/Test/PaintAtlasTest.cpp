#include <cmath>

#include <gtest/gtest.h>

#include "PaintAtlasBaker.hpp"
#include "PaintIslandLayout.hpp"
#include "PaintMeshes.hpp"

using namespace Crowy;

// MintChoco's Tests/PaintAtlasTest.cpp, case for case
namespace
{
    constexpr f32 TexelCm = 0.5f;
    constexpr i32 Pad = 8;
    constexpr i32 MinSize = 256;
    constexpr i32 MaxSize = 2048;

    // a 100 cm cube on the origin with outward corner normals
    PaintAtlasBakeInput makeCube(u8 mask) {
        PaintAtlasBakeInput input;
        for(i32 corner = 0; corner < 8; ++corner) {
            const Vec3 p{
                (corner & 1) ? 50.0f : -50.0f,
                (corner & 2) ? 50.0f : -50.0f,
                (corner & 4) ? 50.0f : -50.0f
            };
            input.positions.push_back(p);
            input.normals.push_back(normalize(p));
        }
        constexpr i32 Faces[6][4]{
            {1, 3, 7, 5},
            {0, 4, 6, 2},
            {2, 6, 7, 3},
            {0, 1, 5, 4},
            {4, 5, 7, 6},
            {0, 2, 3, 1},
        };
        for(const auto& face: Faces) {
            for(const auto i:
                {face[0], face[1], face[2], face[0], face[2], face[3]})
                input.indices.push_back(static_cast<u32>(i));
        }
        input.localBounds.Add({-50.0, -50.0, -50.0});
        input.localBounds.Add({50.0, 50.0, 50.0});
        input.layout = PaintIslandLayout::Build(
            input.localBounds,
            {1.0, 1.0, 1.0},
            mask,
            TexelCm,
            Pad,
            MinSize,
            MaxSize
        );

        return input;
    }

    // two horizontal quads over one footprint: z = low facing down, z =
    // high facing up
    PaintAtlasBakeInput makeStackedQuads(f32 low, f32 high) {
        PaintAtlasBakeInput input;
        const f32 z[2]{low, high};
        for(u32 quad = 0; quad < 2; ++quad) {
            for(i32 corner = 0; corner < 4; ++corner) {
                input.positions.push_back(
                    {(corner & 1) ? 50.0f : -50.0f,
                     (corner & 2) ? 50.0f : -50.0f,
                     z[quad]}
                );
                input.normals.push_back(
                    quad == 0 ? Vec3{0.0f, 0.0f, -1.0f} : Vec3{0.0f, 0.0f, 1.0f}
                );
            }
            const auto base = quad * 4;
            for(const auto i:
                {base, base + 1, base + 3, base, base + 3, base + 2})
                input.indices.push_back(i);
        }
        input.localBounds.Add({-50.0, -50.0, low});
        input.localBounds.Add({50.0, 50.0, high});

        return input;
    }

    usize texelIndex(const PaintIsland& island, DVec3 normalized, i32 atlas) {
        const auto texel = island.ProjectNormalized(normalized);

        return static_cast<usize>(std::floor(texel.y)) * atlas +
               static_cast<usize>(std::floor(texel.x));
    }
}

TEST(PaintAtlas, LayoutSingleIsland) {
    const auto input = makeCube(paintDirectionBit(PaintFaceDirection::Up));
    const auto& layout = input.layout;

    ASSERT_EQ(layout.islands.size(), 1u);
    EXPECT_NEAR(layout.texelCm, TexelCm, 1e-5f);
    // 216 texels round up to 256
    EXPECT_EQ(layout.atlasSize, 256);
    const auto* up = layout.Find(PaintFaceDirection::Up);
    ASSERT_NE(up, nullptr);
    EXPECT_EQ(up->axis, 2);
    EXPECT_EQ(up->sign, 1);
    EXPECT_EQ(up->rect, (IntRect{{0, 0}, {216, 216}}));
    EXPECT_EQ(up->contentOrigin, (IntPoint{Pad, Pad}));
    EXPECT_NEAR(up->contentTexels.x, 200.0, 1e-3);
    EXPECT_NEAR(up->contentTexels.y, 200.0, 1e-3);
    const auto param = up->ToShaderParam(256);
    EXPECT_NEAR(param.x, 8.0f / 256, 1e-5f);
    EXPECT_NEAR(param.y, 8.0f / 256, 1e-5f);
    EXPECT_NEAR(param.z, 200.0f / 256, 1e-5f);
    EXPECT_NEAR(param.w, 200.0f / 256, 1e-5f);
    EXPECT_EQ(layout.Find(PaintFaceDirection::Down), nullptr);
}

TEST(PaintAtlas, GamePadIsThirteen) {
    Box3d bounds;
    bounds.Add({-50.0, -50.0, -50.0});
    bounds.Add({50.0, 50.0, 50.0});
    const auto layout = PaintIslandLayout::Build(
        bounds,
        {1.0, 1.0, 1.0},
        paintDirectionBit(PaintFaceDirection::Up),
        PaintTexelSizeCm,
        PaintIslandPad,
        PaintMinRenderTargetSize,
        PaintMaxRenderTargetSize
    );
    EXPECT_EQ(PaintIslandPad, 13);
    EXPECT_EQ(layout.atlasSize, 256);
    EXPECT_EQ(layout.islands[0].rect, (IntRect{{0, 0}, {226, 226}}));
}

TEST(PaintAtlas, LayoutAllSixFallsBack) {
    // six 216-texel islands need more than 512 on a side: the texel grows
    Box3d bounds;
    bounds.Add({-50.0, -50.0, -50.0});
    bounds.Add({50.0, 50.0, 50.0});
    const auto build = [&] {
        return PaintIslandLayout::Build(
            bounds,
            {1.0, 1.0, 1.0},
            PaintAllDirectionsMask,
            TexelCm,
            Pad,
            MinSize,
            512
        );
    };
    const auto layout = build();

    EXPECT_EQ(layout.islands.size(), 6u);
    EXPECT_GT(layout.atlasSize, 0);
    EXPECT_LE(layout.atlasSize, 512);
    EXPECT_GT(layout.texelCm, TexelCm);
    for(usize a = 0; a < layout.islands.size(); ++a) {
        const auto& ra = layout.islands[a].rect;
        EXPECT_TRUE(
            ra.min.x >= 0 && ra.min.y >= 0 && ra.max.x <= layout.atlasSize &&
            ra.max.y <= layout.atlasSize
        );
        for(auto b = a + 1; b < layout.islands.size(); ++b) {
            const auto& rb = layout.islands[b].rect;
            const bool overlap = ra.min.x < rb.max.x && rb.min.x < ra.max.x &&
                                 ra.min.y < rb.max.y && rb.min.y < ra.max.y;
            EXPECT_FALSE(overlap);
        }
    }
    EXPECT_EQ(layout.ComputeHash(), build().ComputeHash());
}

TEST(PaintAtlas, FloorTexelGrows) {
    // the stage floor, 20 m square: 4026 texels never fit 2048
    Box3d bounds;
    bounds.Add({-50.0, -50.0, -50.0});
    bounds.Add({50.0, 50.0, 50.0});
    const auto layout = PaintIslandLayout::Build(
        bounds,
        {20.0, 20.0, 0.2},
        paintDirectionBit(PaintFaceDirection::Up),
        PaintTexelSizeCm,
        PaintIslandPad,
        PaintMinRenderTargetSize,
        PaintMaxRenderTargetSize
    );
    const auto area = 4026.0 * 4026.0;
    const auto grown = static_cast<f32>(
        PaintTexelSizeCm * std::sqrt(area / (0.85 * 2048.0 * 2048.0))
    );

    EXPECT_EQ(layout.atlasSize, 2048);
    EXPECT_FLOAT_EQ(layout.texelCm, grown);
    EXPECT_EQ(layout.islands[0].rect.Width(), 1902);
}

TEST(PaintAtlas, RasterUnitCubeUp) {
    const auto input = makeCube(paintDirectionBit(PaintFaceDirection::Up));
    const auto& up = input.layout.islands[0];
    const auto n = input.layout.atlasSize;

    std::vector<Vec4> positions;
    PaintAtlasBaker::rasterize(input, positions);
    ASSERT_EQ(positions.size(), static_cast<usize>(n * n));

    // the texel under the top's centre holds the top's centre
    const auto& center = positions[texelIndex(up, {0.5, 0.5, 1.0}, n)];
    EXPECT_NEAR(center.x, 0.5f, 0.01f);
    EXPECT_NEAR(center.y, 0.5f, 0.01f);
    EXPECT_NEAR(center.z, 1.0f, 1e-3f);
    EXPECT_NEAR(center.w, 1.0f, 1e-6f);

    i32 uncovered = 0;
    for(auto y = up.contentOrigin.y; y < up.contentOrigin.y + 200; ++y)
        for(auto x = up.contentOrigin.x; x < up.contentOrigin.x + 200; ++x)
            uncovered +=
                positions[static_cast<usize>(y * n + x)].w > 0.5f ? 0 : 1;
    EXPECT_EQ(uncovered, 0);

    const auto& gutter = positions[static_cast<usize>(
        (up.contentOrigin.y + 100) * n + up.contentOrigin.x - 1
    )];
    EXPECT_NEAR(gutter.w, 0.0f, 1e-6f);
    EXPECT_NEAR(gutter.x, PaintAtlasBaker::EmptyPosition, 1e-6f);
}

TEST(PaintAtlas, DepthKeepsOutermost) {
    auto input = makeStackedQuads(0.0f, 50.0f);
    const auto mask = static_cast<u8>(
        paintDirectionBit(PaintFaceDirection::Up) |
        paintDirectionBit(PaintFaceDirection::Down)
    );
    input.layout = PaintIslandLayout::Build(
        input.localBounds,
        {1.0, 1.0, 1.0},
        mask,
        TexelCm,
        Pad,
        MinSize,
        MaxSize
    );
    const auto n = input.layout.atlasSize;

    std::vector<Vec4> positions;
    PaintAtlasBaker::rasterize(input, positions);

    // from above the upper quad wins; from below the lower, the only one
    // facing that way; normalized z is 0 for z = 0 and 1 for z = 50
    const auto* up = input.layout.Find(PaintFaceDirection::Up);
    const auto* down = input.layout.Find(PaintFaceDirection::Down);
    ASSERT_NE(up, nullptr);
    ASSERT_NE(down, nullptr);
    EXPECT_NEAR(positions[texelIndex(*up, {0.5, 0.5, 1.0}, n)].z, 1.0f, 1e-3f);
    EXPECT_NEAR(
        positions[texelIndex(*down, {0.5, 0.5, 0.0}, n)].z,
        0.0f,
        1e-3f
    );
}

TEST(PaintAtlas, EdgeFadeRampsFromEveryEdge) {
    auto input = makeCube(paintDirectionBit(PaintFaceDirection::Up));
    PaintAtlasBakeOutput output;
    PaintAtlasBaker::bake(input, output);
    const auto n = input.layout.atlasSize;
    const auto& up = input.layout.islands[0];
    const auto at = [&](i32 x, i32 y) {
        return output.edgeFade[static_cast<usize>(y * n + x)];
    };
    const auto cx = up.contentOrigin.x + 100;
    const auto cy = up.contentOrigin.y + 100;

    EXPECT_EQ(output.coveredTexels, 200 * 200);
    EXPECT_EQ(at(cx, cy), 255);
    EXPECT_EQ(at(up.contentOrigin.x, cy), 0);
    EXPECT_EQ(at(up.contentOrigin.x - 1, cy), 0);
    for(i32 d = 1; d <= 8; ++d)
        EXPECT_GE(
            at(up.contentOrigin.x + d, cy),
            at(up.contentOrigin.x + d - 1, cy)
        );
    EXPECT_EQ(at(up.contentOrigin.x + 8, cy), 255);
}

TEST(PaintAtlas, HalfRoundsToNearestEven) {
    EXPECT_EQ(toHalf(0.0f), 0x0000);
    EXPECT_EQ(toHalf(1.0f), 0x3C00);
    EXPECT_EQ(toHalf(-64.0f), 0xD400);
    EXPECT_EQ(toHalf(0.5f), 0x3800);
    // 1 + 2^-11 is halfway between 1 and the next half: it rounds to even
    EXPECT_EQ(toHalf(1.0f + 1.0f / 2048.0f), 0x3C00);
    EXPECT_EQ(toHalf(1.0f + 3.0f / 2048.0f), 0x3C02);
    EXPECT_EQ(toHalf(65520.0f), 0x7C00);
}
