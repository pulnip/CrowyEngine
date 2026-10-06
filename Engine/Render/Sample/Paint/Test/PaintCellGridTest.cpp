#include <gtest/gtest.h>

#include "PaintCellGrid.hpp"

using namespace Crowy;

// MintChoco's Tests/PaintCellGridTest.cpp, case for case
namespace
{
    // a 100 cm cube on the origin: 8 shared corners, 12 triangles, outward
    // corner normals; the grid ignores winding, the normals fix the sign
    struct UnitCube {
        std::vector<Vec3> positions;
        std::vector<Vec3> normals;
        std::vector<u32> indices;
        Box3d bounds;

        UnitCube() {
            for(i32 corner = 0; corner < 8; ++corner) {
                const Vec3 p{
                    (corner & 1) ? 50.0f : -50.0f,
                    (corner & 2) ? 50.0f : -50.0f,
                    (corner & 4) ? 50.0f : -50.0f
                };
                positions.push_back(p);
                normals.push_back(normalize(p));
            }
            constexpr i32 Faces[6][4]{
                {1, 3, 7, 5},
                {0, 4, 6, 2},
                {2, 6, 7, 3},
                {0, 1, 5, 4},
                {4, 5, 7, 6},
                {0, 2, 3, 1},
            };
            for(const auto& f: Faces) {
                for(const auto i: {f[0], f[1], f[2], f[0], f[2], f[3]})
                    indices.push_back(static_cast<u32>(i));
            }
            bounds.Add({-50.0, -50.0, -50.0});
            bounds.Add({50.0, 50.0, 50.0});
        }

        void Build(PaintCellGrid& grid, f32 cell) const {
            grid.Build(bounds, cell, positions, normals, indices);
        }
    };

    PaintLocalStamp topStamp() {
        return PaintLocalStamp{
            .center = {0.0, 0.0, 50.0},
            .axisU = {1.0, 0.0, 0.0},
            .axisV = {0.0, 1.0, 0.0},
            .normal = {0.0, 0.0, 1.0},
            .radius = 60.0f,
            .stretch = 1.0f
        };
    }
}

TEST(PaintCellGrid, UnitCube) {
    PaintCellGrid grid;
    UnitCube().Build(grid, 25.0f);

    EXPECT_EQ(grid.Dims(), (std::array<i32, 3>{4, 4, 4}));
    // six faces of 4 x 4
    EXPECT_EQ(grid.SurfaceCellCount(), 96);
    EXPECT_NEAR(grid.Coverage().totalArea, 60000.0f, 60.0f);
    for(u8 d = 0; d < PaintFaceDirectionCount; ++d) {
        const auto face = grid.Coverage(paintFaceDirectionAt(d));
        EXPECT_NEAR(face.totalArea, 10000.0f, 10.0f);
        EXPECT_NEAR(face.Fraction(PaintIdNone), 1.0f, 1e-4f);
    }
}

TEST(PaintCellGrid, MarkTop) {
    PaintCellGrid grid;
    UnitCube().Build(grid, 25.0f);

    // half the radius across the surface reaches the four centre cells of
    // the top, and the full radius along the normal stops short of the rest
    EXPECT_EQ(grid.Mark(topStamp(), 1, 0.5f), 4);
    EXPECT_NEAR(
        grid.Coverage(PaintFaceDirection::Up).Fraction(1),
        0.25f,
        1e-3f
    );
    using enum PaintFaceDirection;
    for(const auto other: {Front, Back, Right, Left, Down})
        EXPECT_NEAR(grid.Coverage(other).Fraction(1), 0.0f, 1e-6f);
    EXPECT_NEAR(grid.Coverage().Fraction(1), 2500.0f / 60000.0f, 1e-4f);

    // painting the none id is the eraser
    EXPECT_EQ(grid.Mark(topStamp(), PaintIdNone, 0.5f), 4);
    EXPECT_NEAR(grid.Coverage(Up).Fraction(PaintIdNone), 1.0f, 1e-4f);
}

TEST(PaintCellGrid, MaskAndScale) {
    // scaled-local positions, world-sized cells, classification with the
    // scale multiplied back in; only the enabled directions get cells
    const DVec3 scale{2.0, 1.0, 1.0};
    UnitCube cube;
    for(auto& p: cube.positions)
        p = p * toVec3(scale);
    Box3d bounds;
    bounds.Add({-100.0, -50.0, -50.0});
    bounds.Add({100.0, 50.0, 50.0});
    const auto mask = static_cast<u8>(
        paintDirectionBit(PaintFaceDirection::Up) |
        paintDirectionBit(PaintFaceDirection::Front)
    );

    PaintCellGrid grid;
    grid.Build(
        bounds,
        25.0f,
        cube.positions,
        cube.normals,
        cube.indices,
        mask,
        scale
    );

    EXPECT_EQ(grid.Dims(), (std::array<i32, 3>{8, 4, 4}));
    // top 8 x 4 plus front 4 x 4
    EXPECT_EQ(grid.SurfaceCellCount(), 48);
    EXPECT_NEAR(grid.Coverage().totalArea, 30000.0f, 30.0f);
    EXPECT_NEAR(
        grid.Coverage(PaintFaceDirection::Up).totalArea,
        20000.0f,
        20.0f
    );
    EXPECT_NEAR(
        grid.Coverage(PaintFaceDirection::Front).totalArea,
        10000.0f,
        10.0f
    );
    using enum PaintFaceDirection;
    for(const auto disabled: {Back, Right, Left, Down})
        EXPECT_NEAR(grid.Coverage(disabled).totalArea, 0.0f, 1e-6f);

    EXPECT_EQ(grid.Mark(topStamp(), 1, 0.5f), 4);
    EXPECT_NEAR(grid.Coverage(Up).Fraction(1), 2500.0f / 20000.0f, 1e-3f);
}

TEST(PaintCellGrid, StarLock) {
    PaintCellGrid grid;
    UnitCube().Build(grid, 25.0f);
    const auto count = [&grid](u8 paintId, u8 starGen) {
        i32 n = 0;
        grid.ForEachSurfaceCell(
            [&](Vec3, PaintFaceDirection, u8 id, u8 gen, f32) {
                n += id == paintId && gen == starGen ? 1 : 0;
            }
        );
        return n;
    };

    // a star's trail: team 0, generation 1, on the top's four centre cells
    PaintLockGens locks;
    EXPECT_EQ(grid.Mark(topStamp(), 0, 1, locks, 0.5f), 4);
    EXPECT_EQ(count(0, 1), 4);

    // while that generation is locked no other id takes them
    locks.gen[0] = 1;
    EXPECT_EQ(grid.Mark(topStamp(), 1, 0, locks, 0.5f), 0);
    EXPECT_NEAR(
        grid.Coverage(PaintFaceDirection::Up).Fraction(0),
        0.25f,
        1e-3f
    );

    // its own id paints over and keeps the generation the lock is keyed on
    EXPECT_EQ(grid.Mark(topStamp(), 0, 0, locks, 0.5f), 0);
    EXPECT_EQ(count(0, 1), 4);

    // once released, the enemy takes them
    locks.gen[0] = 0;
    EXPECT_EQ(grid.Mark(topStamp(), 1, 0, locks, 0.5f), 4);
}

TEST(PaintCellGrid, FacesTurnedAwayStayAsTheyWere) {
    PaintCellGrid grid;
    UnitCube().Build(grid, 25.0f);
    // a stamp on the top whose ellipsoid reaches through to the bottom
    auto stamp = topStamp();
    stamp.radius = 120.0f;
    grid.Mark(stamp, 0, 1.0f);
    EXPECT_NEAR(
        grid.Coverage(PaintFaceDirection::Down).Fraction(0),
        0.0f,
        1e-6f
    );
    EXPECT_GT(grid.Coverage(PaintFaceDirection::Up).Fraction(0), 0.9f);
}

TEST(PaintTexel, IdAndGenerationRoundTrip) {
    for(u8 id = 0; id < PaintIdCount; ++id) {
        for(u8 gen = 0; gen <= PaintStarGenMax + 1; ++gen) {
            const auto texel = encodePaintTexel(id, gen);
            EXPECT_EQ(decodePaintId(texel), id);
            // no id carries no generation; a generation past the top clamps
            const u8 expected =
                id == PaintIdNone ? 0 : std::min<u8>(gen, PaintStarGenMax);
            EXPECT_EQ(decodePaintStarGen(texel), expected);
        }
    }
}
