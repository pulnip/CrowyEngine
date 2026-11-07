#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <format>
#include <map>
#include <vector>

#include <gtest/gtest.h>

#include "ImageLoader.hpp"
#include "ImageView.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "ModelLoader.hpp"
#include "PathUtil.hpp"
#include "ProjectRoot.hpp"

using namespace Crowy;

namespace
{
    struct Triangle {
        std::array<Vertex, 3> corners;
    };

    const auto testData = toPath(CROWY_CONTENT_TEST_DATA);

    std::vector<Triangle> trianglesOf(const MeshData& mesh) {
        std::vector<Triangle> triangles;
        for(usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
            triangles.push_back(
                Triangle{
                    {mesh.vertices[mesh.indices[i]],
                     mesh.vertices[mesh.indices[i + 1]],
                     mesh.vertices[mesh.indices[i + 2]]}
                }
            );
        }

        return triangles;
    }

    // the winding normal's agreement with the stored one: +1 or -1 for a
    // triangle whose corners are not collinear
    f32 windingSign(const Triangle& t) {
        const auto& [a, b, c] = t.corners;
        const auto winding =
            cross(b.position - a.position, c.position - a.position);

        return dot(winding, a.normal) > 0.0f ? 1.0f : -1.0f;
    }

    Vec3 centroidOf(const Triangle& t) {
        const auto& [a, b, c] = t.corners;

        return (a.position + b.position + c.position) / 3.0f;
    }

    // the texel a face's UVs name, as #RRGGBB
    u32 paletteColor(const Rgba8View& palette, Vec2 uv) {
        const auto x = static_cast<u32>(uv.x * static_cast<f32>(palette.width));
        const auto y =
            static_cast<u32>(uv.y * static_cast<f32>(palette.height));
        const auto* texel = palette.pixels + y * palette.rowPitch + x * 4;

        return (u32{texel[0]} << 16) | (u32{texel[1]} << 8) | u32{texel[2]};
    }

    struct Extent {
        Vec3 low{1e9f, 1e9f, 1e9f};
        Vec3 high{-1e9f, -1e9f, -1e9f};
    };

    void extend(Extent& extent, Vec3 p) {
        auto& [low, high] = extent;
        low = Vec3{
            std::min(low.x, p.x),
            std::min(low.y, p.y),
            std::min(low.z, p.z)
        };
        high = Vec3{
            std::max(high.x, p.x),
            std::max(high.y, p.y),
            std::max(high.z, p.z)
        };
    }
}

// MakeBox is the convention's reference: every one of its triangles winds
// the same way relative to its outward normal, and that way is "front".
TEST(ModelLoader, MakeBoxDefinesTheFrontWinding) {
    const auto box = MakeBox(0.5f);
    for(const auto& triangle: trianglesOf(box))
        EXPECT_EQ(windingSign(triangle), windingSign(trianglesOf(box).front()));
}

// the axis probe's table from the content's contract
TEST(ModelLoader, AxisProbeMatchesItsTable) {
    constexpr f32 Tolerance = 1e-4f;

    const auto model = loadModel(testData / "AxisProbe.fbx");

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

    auto image = LoadImage(testData / "Palette.png");
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
    EXPECT_GT(
        shortArm.high.x - shortArm.low.x,
        shortArm.high.z - shortArm.low.z
    );
}

TEST(ModelLoader, AxisProbeNormalsPointOutward) {
    const auto model = loadModel(testData / "AxisProbe.fbx");
    auto image = LoadImage(testData / "Palette.png");
    const auto palette = viewRgba8(image);

    std::map<u32, Extent> extents;
    std::vector<std::pair<u32, Triangle>> triangles;
    for(const auto& slot: model.slots) {
        for(const auto& triangle: trianglesOf(slot.mesh)) {
            const auto color =
                paletteColor(palette, triangle.corners[0].texCoord);
            for(const auto& corner: triangle.corners)
                extend(extents[color], corner.position);
            triangles.emplace_back(color, triangle);
        }
    }

    for(const auto& [color, triangle]: triangles) {
        const auto& extent = extents[color];
        const auto boxCenter = 0.5f * (extent.low + extent.high);
        EXPECT_GT(
            dot(triangle.corners[0].normal, centroidOf(triangle) - boxCenter),
            0.0f
        );
    }
}

// the node probe's three boxes as Blender holds them, each in its own space
TEST(ModelLoader, NodeProbeKeepsItsHierarchy) {
    constexpr f32 Tolerance = 1e-4f;

    const auto model = loadModel(testData / "NodeProbe.fbx");

    ASSERT_EQ(model.nodes.size(), 3u);
    const auto& base = model.nodes[0];
    const auto& arm = model.nodes[1];
    const auto& tip = model.nodes[2];
    EXPECT_EQ(base.name, "Base");
    EXPECT_EQ(arm.name, "Arm");
    EXPECT_EQ(tip.name, "Tip");
    EXPECT_EQ(base.parent, ModelNode::NoParent);
    EXPECT_EQ(arm.parent, 0u);
    EXPECT_EQ(tip.parent, 1u);

    const auto expectNear = [](Vec3 actual, Vec3 expected) {
        EXPECT_NEAR(actual.x, expected.x, Tolerance);
        EXPECT_NEAR(actual.y, expected.y, Tolerance);
        EXPECT_NEAR(actual.z, expected.z, Tolerance);
    };
    // a node's space is the file's with Z negated; the top node turns the
    // file's front, +Y, to +Z and its up to +Y
    expectNear(base.local.position, zeros());
    expectNear(rotate(unitY(), base.local.rotation), unitZ());
    expectNear(rotate(-unitZ(), base.local.rotation), unitY());
    expectNear(arm.local.position, Vec3{0.1f, 0.0f, -0.1f});
    expectNear(rotate(unitX(), arm.local.rotation), unitY());
    expectNear(arm.local.scale, ones());
    // the tip's mirror along its own X, however the file splits it
    expectNear(
        transformPoint(modelMat(tip.local), Vec3{1.0f, 1.0f, 1.0f}),
        Vec3{-0.7f, 1.0f, 1.0f}
    );

    const auto expectBox =
        [&](const ModelNode& node, StrView material, Vec3 low, Vec3 high) {
            ASSERT_EQ(node.slots.size(), 1u) << node.name;
            const auto& slot = node.slots.front();
            EXPECT_EQ(slot.material, material);
            EXPECT_EQ(slot.mesh.indices.size(), 36u);
            expectNear(slot.bounds.center - slot.bounds.halfScale, low);
            expectNear(slot.bounds.center + slot.bounds.halfScale, high);
        };
    expectBox(base, "Steel", Vec3{-0.2f, -0.2f, -0.1f}, Vec3{0.2f, 0.2f, 0.0f});
    expectBox(
        arm,
        "Paint",
        Vec3{0.0f, -0.025f, -0.05f},
        Vec3{0.3f, 0.025f, 0.0f}
    );
    expectBox(tip, "Steel", Vec3{0.0f, -0.05f, -0.1f}, Vec3{0.1f, 0.05f, 0.0f});

    ASSERT_EQ(model.slots.size(), 2u);
    for(const auto& slot: model.slots) {
        EXPECT_EQ(
            slot.mesh.indices.size(),
            slot.material == "Steel" ? 72u : 36u
        ) << slot.material;
    }
}

// each node's slots, carried up its chain of local transforms, are the root
// slots' triangles with the same winding, the mirrored tip's included
TEST(ModelLoader, NodeSlotsPlacedByTheirChainAreTheRootSlots) {
    constexpr f32 Tolerance = 1e-4f;

    const auto model = loadModel(testData / "NodeProbe.fbx");

    std::vector<Mat4> toRoot;
    std::map<Str, std::vector<Triangle>> placed;
    for(const auto& node: model.nodes) {
        const auto local = modelMat(node.local);
        toRoot.push_back(
            node.parent == ModelNode::NoParent ? local
                                               : toRoot[node.parent] * local
        );
        const auto& matrix = toRoot.back();
        for(const auto& slot: node.slots) {
            for(auto triangle: trianglesOf(slot.mesh)) {
                for(auto& corner: triangle.corners) {
                    corner.position = transformPoint(matrix, corner.position);
                    corner.normal = normalize(transformDirection(
                        transpose(inverseAffine(matrix)),
                        corner.normal
                    ));
                }
                placed[slot.material].push_back(triangle);
            }
        }
    }

    const auto sameCorner = [](const Vertex& a, const Vertex& b) {
        return norm(a.position - b.position) < Tolerance &&
               norm(a.normal - b.normal) < Tolerance;
    };
    // a turn of the corners keeps the winding, a swap of two does not
    const auto sameTriangle = [&](const Triangle& a, const Triangle& b) {
        for(usize turn = 0; turn < 3; ++turn) {
            if(sameCorner(a.corners[0], b.corners[turn]) &&
               sameCorner(a.corners[1], b.corners[(turn + 1) % 3]) &&
               sameCorner(a.corners[2], b.corners[(turn + 2) % 3]))
                return true;
        }
        return false;
    };

    for(const auto& slot: model.slots) {
        const auto& candidates = placed[slot.material];
        const auto triangles = trianglesOf(slot.mesh);
        EXPECT_EQ(candidates.size(), triangles.size()) << slot.material;
        for(const auto& triangle: triangles) {
            EXPECT_TRUE(std::ranges::any_of(candidates, [&](const Triangle& c) {
                return sameTriangle(triangle, c);
            })) << std::format("{} at {}", slot.material, centroidOf(triangle));
        }
    }
}

// Tools/gen.py's output as the engine reads it: a moving part is the root's
// child with its origin on the card's anchor, the slots in the card's palette
TEST(ModelLoader, GeneratedPropPartSitsOnItsAnchor) {
    constexpr f32 Tolerance = 1e-4f;

    const auto model =
        loadModel(projectRoot() / "Content/Props/Interact/FloorLever.fbx");

    ASSERT_EQ(model.nodes.size(), 2u);
    const auto& root = model.nodes[0];
    const auto& handle = model.nodes[1];
    EXPECT_EQ(root.name, "FloorLever");
    EXPECT_EQ(handle.name, "Handle");
    EXPECT_EQ(handle.parent, 0u);

    // the card's anchor (0, 0, 0.45) is Z up
    const auto anchor =
        transformPoint(modelMat(root.local), handle.local.position);
    EXPECT_NEAR(anchor.x, 0.0f, Tolerance);
    EXPECT_NEAR(anchor.y, 0.45f, Tolerance);
    EXPECT_NEAR(anchor.z, 0.0f, Tolerance);

    const std::array palette{"Stone", "Bronze", "Iron", "Glow"};
    ASSERT_EQ(model.slots.size(), palette.size());
    for(usize i = 0; i < palette.size(); ++i)
        EXPECT_EQ(model.slots[i].material, palette[i]);
}

TEST(ModelLoader, MissingFileThrows) {
    EXPECT_THROW(loadModel(testData / "NoSuchModel.fbx"), std::runtime_error);
}
