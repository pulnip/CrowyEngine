#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <map>
#include <vector>

#include <gtest/gtest.h>

#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "ModelLoader.hpp"

using namespace Crowy;

namespace
{
    constexpr f32 Tolerance = 1e-4f;

    struct Triangle {
        std::array<Vertex, 3> corners;
    };

    const std::filesystem::path TestData{CROWY_SERIALIZATION_TEST_DATA};

    std::vector<Triangle> trianglesOf(const MeshData& mesh) {
        std::vector<Triangle> triangles;
        for(usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
            triangles.push_back(Triangle{{
                mesh.vertices[mesh.indices[i]],
                mesh.vertices[mesh.indices[i + 1]],
                mesh.vertices[mesh.indices[i + 2]]
            }});
        }

        return triangles;
    }

    // the winding normal's agreement with the stored one: +1 or -1 for a
    // triangle whose corners are not collinear
    f32 windingSign(const Triangle& t) {
        const auto& [a, b, c] = t.corners;
        const auto winding = cross(b.position - a.position, c.position - a.position);

        return dot(winding, a.normal) > 0.0f ? 1.0f : -1.0f;
    }

    Vec3 centroidOf(const Triangle& t) {
        const auto& [a, b, c] = t.corners;

        return (a.position + b.position + c.position) / 3.0f;
    }

    // the texel a face's UVs name, as #RRGGBB
    u32 paletteColor(const Rgba8View& palette, Vec2 uv) {
        const auto x = static_cast<u32>(uv.x * static_cast<f32>(palette.width));
        const auto y = static_cast<u32>(uv.y * static_cast<f32>(palette.height));
        const auto* texel = palette.pixels + y * palette.rowPitch + x * 4;

        return (u32{texel[0]} << 16) | (u32{texel[1]} << 8) | u32{texel[2]};
    }

    struct Extent {
        Vec3 low{1e9f, 1e9f, 1e9f};
        Vec3 high{-1e9f, -1e9f, -1e9f};
    };

    void extend(Extent& extent, Vec3 p) {
        auto& [low, high] = extent;
        low = Vec3{std::min(low.x, p.x), std::min(low.y, p.y), std::min(low.z, p.z)};
        high = Vec3{std::max(high.x, p.x), std::max(high.y, p.y), std::max(high.z, p.z)};
    }
}

// MakeBox is the convention's reference: every one of its triangles winds
// the same way relative to its outward normal, and that way is "front".
TEST(ModelLoader, MakeBoxDefinesTheFrontWinding) {
    const auto box = MakeBox(0.5f);
    for(const auto& triangle: trianglesOf(box))
        EXPECT_EQ(windingSign(triangle), windingSign(trianglesOf(box).front()));
}

// The axis probe's table from the content's own contract: bounds, the
// arms' directions and lengths, the triangle count, front faces and the
// palette colours after the v flip.
TEST(ModelLoader, AxisProbeMatchesItsTable) {
    const auto model = LoadModel(TestData / "AxisProbe.fbx");

    const auto low = model.bounds.center - model.bounds.halfScale;
    const auto high = model.bounds.center + model.bounds.halfScale;
    EXPECT_NEAR(low.x, -0.1f, Tolerance);
    EXPECT_NEAR(low.y, 0.0f, Tolerance);
    EXPECT_NEAR(low.z, -0.1f, Tolerance);
    EXPECT_NEAR(high.x, 0.3f, Tolerance);
    EXPECT_NEAR(high.y, 0.2f, Tolerance);
    EXPECT_NEAR(high.z, 0.5f, Tolerance);

    std::vector<Triangle> triangles;
    for(const auto& slot: model.slots) {
        for(const auto& triangle: trianglesOf(slot.mesh))
            triangles.push_back(triangle);
    }
    ASSERT_EQ(triangles.size(), 36u);

    const auto front = windingSign(trianglesOf(MakeBox(0.5f)).front());
    for(const auto& triangle: triangles)
        EXPECT_EQ(windingSign(triangle), front);

    auto image = LoadImage(TestData / "Palette.png");
    const auto palette = viewRgba8(image);

    std::map<u32, Extent> extents;
    std::map<u32, u32> triangleCounts;
    for(const auto& triangle: triangles) {
        const auto color = paletteColor(palette, triangle.corners[0].texCoord);
        ++triangleCounts[color];
        for(const auto& corner: triangle.corners)
            extend(extents[color], corner.position);
    }

    constexpr u32 Core = 0xF2B705;
    constexpr u32 LongArm = 0xF2665E;
    constexpr u32 ShortArm = 0x1FA6A0;
    ASSERT_EQ(triangleCounts.size(), 3u);
    ASSERT_TRUE(triangleCounts.contains(Core));
    ASSERT_TRUE(triangleCounts.contains(LongArm));
    ASSERT_TRUE(triangleCounts.contains(ShortArm));

    const auto& core = extents[Core];
    EXPECT_NEAR(core.low.x, -0.1f, Tolerance);
    EXPECT_NEAR(core.high.x, 0.1f, Tolerance);
    EXPECT_NEAR(core.low.z, -0.1f, Tolerance);
    EXPECT_NEAR(core.high.z, 0.1f, Tolerance);
    EXPECT_NEAR(core.low.y, 0.0f, Tolerance);
    EXPECT_NEAR(core.high.y, 0.2f, Tolerance);

    // the long arm reaches +Z; a swapped pair of arms is an unconverted
    // handedness, an arm on a negative axis a rotation instead of the swap
    const auto& longArm = extents[LongArm];
    EXPECT_NEAR(longArm.high.z, 0.5f, Tolerance);
    EXPECT_GT(longArm.high.z - longArm.low.z, longArm.high.x - longArm.low.x);

    const auto& shortArm = extents[ShortArm];
    EXPECT_NEAR(shortArm.high.x, 0.3f, Tolerance);
    EXPECT_GT(shortArm.high.x - shortArm.low.x, shortArm.high.z - shortArm.low.z);
}

// "front-facing from outside": every triangle's stored normal points away
// from the centre of the box it belongs to
TEST(ModelLoader, AxisProbeNormalsPointOutward) {
    const auto model = LoadModel(TestData / "AxisProbe.fbx");
    auto image = LoadImage(TestData / "Palette.png");
    const auto palette = viewRgba8(image);

    std::map<u32, Extent> extents;
    std::vector<std::pair<u32, Triangle>> triangles;
    for(const auto& slot: model.slots) {
        for(const auto& triangle: trianglesOf(slot.mesh)) {
            const auto color = paletteColor(palette, triangle.corners[0].texCoord);
            for(const auto& corner: triangle.corners)
                extend(extents[color], corner.position);
            triangles.emplace_back(color, triangle);
        }
    }

    for(const auto& [color, triangle]: triangles) {
        const auto& extent = extents[color];
        const auto boxCenter = 0.5f * (extent.low + extent.high);
        EXPECT_GT(dot(triangle.corners[0].normal, centroidOf(triangle) - boxCenter), 0.0f);
    }
}

TEST(ModelLoader, MissingFileThrows) {
    EXPECT_THROW(LoadModel(TestData / "NoSuchModel.fbx"), std::runtime_error);
}
