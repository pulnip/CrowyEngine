#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "EditorPick.hpp"
#include "MeshGenerator.hpp"

using namespace Crowy;

namespace
{
    using MeshPointers = std::vector<const MeshData*>;

    // a camera at the origin looking along +Z, 16:9 at 1920 x 1080
    constexpr Vec2 Viewport{1920.0f, 1080.0f};

    EditorCamera perspectiveCamera() {
        return EditorCamera{EditorCut{.name = "test", .lens = {.fovY = std::numbers::pi_v<f32> / 3}}};
    }

    // primitives of one mesh each, every one drawing `mesh`
    class Scene {
    public:
        RenderScene scene;
        MeshData box = MakeBox(0.5f);
        MeshData quad = MakePlane(-unitZ(), unitX(), 0.5f);
        MeshPointers boxMeshes{&box};
        MeshPointers quadMeshes{&quad};
        std::vector<PrimitiveHandle> quads;

        PrimitiveHandle Add(const MeshData& mesh, Vec3 position, Vec3 scale = ones()) {
            const auto handle = scene.Meshes().Add(MeshResource{});
            const auto primitive = scene.Primitives().Add(PrimitiveSnapshot{
                .localToWorld = translateMat(position) * scaleMat(scale),
                .worldBounds = AABB3D{.center = position, .halfScale = 0.5f * scale},
                .mesh = handle
            });
            if(&mesh == &quad)
                quads.push_back(primitive);

            return primitive;
        }

        PickMeshes Meshes() const {
            return [this](PrimitiveHandle handle) -> std::span<const MeshData* const> {
                if(std::ranges::find(quads, handle) != quads.end())
                    return quadMeshes;
                return boxMeshes;
            };
        }

        std::optional<PickHit> Pick(const Ray3D& ray) const {
            return pickScene(scene, ray, Meshes());
        }
    };

    Vec2 projectToPixel(const EditorCamera& camera, Vec3 point) {
        const auto clip = camera.ViewProj(Viewport.x / Viewport.y) * Vec4{point.x, point.y, point.z, 1.0f};
        const auto ndcX = clip.x / clip.w;
        const auto ndcY = clip.y / clip.w;

        return Vec2{(ndcX + 1.0f) * 0.5f * Viewport.x, (1.0f - ndcY) * 0.5f * Viewport.y};
    }
}

TEST(EditorPick, CenterPixelLooksForward) {
    const auto camera = perspectiveCamera();
    const auto ray = rayThroughPixel(camera, 0.5f * Viewport, Viewport);

    EXPECT_NEAR(ray.direction.z, 1.0f, 1e-5f);
    EXPECT_NEAR(ray.direction.x, 0.0f, 1e-5f);
}

// a point along the pixel's ray projects back onto the same pixel, either lens
TEST(EditorPick, RayAndProjectionAgree) {
    auto camera = perspectiveCamera();
    camera.yaw = 0.4f;
    camera.pitch = 0.2f;
    camera.position = {1.0f, 2.0f, 3.0f};
    camera.RecomputeView();

    for(const auto orthographic: {false, true}) {
        camera.lens.orthographic = orthographic;
        camera.lens.orthoHalfHeight = 8.0f;
        for(const auto pixel: {Vec2{10.0f, 20.0f}, Vec2{960.0f, 540.0f}, Vec2{1900.0f, 1000.0f}}) {
            const auto ray = rayThroughPixel(camera, pixel, Viewport);
            const auto back = projectToPixel(camera, ray.origin + ray.direction * 12.0f);
            EXPECT_NEAR(back.x, pixel.x, 0.5f) << orthographic;
            EXPECT_NEAR(back.y, pixel.y, 0.5f) << orthographic;
        }
    }
}

TEST(EditorPick, TheNearerBoxWins) {
    Scene s;
    const auto near = s.Add(s.box, {0.0f, 0.0f, 5.0f});
    s.Add(s.box, {0.0f, 0.0f, 9.0f});

    const auto hit = s.Pick(Ray3D{});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->primitive, near);
    EXPECT_NEAR(hit->distance, 4.5f, 1e-4f);
}

// a box's bounds can swallow a ray its triangles never meet: the rotated
// thin plate behind which a box stands
TEST(EditorPick, BoundsHitTrianglesMissFallsThrough) {
    Scene s;
    // a plate whose bounds reach x = 2, though the ray at x = 1.5 passes
    // beside it once it turns: a quad facing -Z scaled to 0.5 wide, its
    // bounds deliberately loose
    const auto plate = s.scene.Primitives().Add(PrimitiveSnapshot{
        .localToWorld = translateMat({0.0f, 0.0f, 3.0f}) * scaleMat({0.5f, 0.5f, 1.0f}),
        .worldBounds = AABB3D{.center = {0.0f, 0.0f, 3.0f}, .halfScale = {2.0f, 2.0f, 0.1f}},
        .mesh = s.scene.Meshes().Add(MeshResource{})
    });
    s.quads.push_back(plate);
    const auto behind = s.Add(s.box, {1.5f, 0.0f, 8.0f}, {1.0f, 1.0f, 1.0f});

    const auto hit = s.Pick(Ray3D{.origin = {1.5f, 0.0f, 0.0f}});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->primitive, behind);
}

TEST(EditorPick, AQuadIsPickedFromItsFrontOnly) {
    Scene s;
    const auto quad = s.Add(s.quad, {0.0f, 0.0f, 4.0f}, {2.0f, 2.0f, 1.0f});

    const auto front = s.Pick(Ray3D{.origin = {0.2f, 0.1f, 0.0f}});
    ASSERT_TRUE(front.has_value());
    EXPECT_EQ(front->primitive, quad);
    EXPECT_FALSE(s.Pick(Ray3D{.origin = {0.2f, 0.1f, 8.0f}, .direction = {0.0f, 0.0f, -1.0f}}));
}

TEST(EditorPick, AHiddenRowIsNotPicked) {
    Scene s;
    const auto hidden = s.Add(s.box, {0.0f, 0.0f, 5.0f});
    const auto behind = s.Add(s.box, {0.0f, 0.0f, 9.0f});
    s.scene.Primitives().GetRef(hidden).flags = PrimitiveFlags::CastShadow;

    const auto hit = s.Pick(Ray3D{});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->primitive, behind);
}

// the landmark: a unit box stretched per axis, picked at its analytic depth
TEST(EditorPick, AStretchedBoxAtItsAnalyticDistance) {
    Scene s;
    const auto landmark = s.Add(s.box, {18.65f, 17.25f, 18.65f}, {14.7f, 34.2f, 14.7f});

    const auto hit = s.Pick(Ray3D{.origin = {18.65f, 10.0f, -20.0f}});
    ASSERT_TRUE(hit.has_value());
    EXPECT_EQ(hit->primitive, landmark);
    EXPECT_NEAR(hit->distance, 20.0f + 18.65f - 7.35f, 1e-3f);
}
