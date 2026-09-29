#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include "FlyCamera.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    // Regression check for position invariance between the depth prepass and
    // the opaque pass that tests Equal against it, described in full in
    // Engine/Render/Spike/PrepassEqualSpike.slang.
    //
    //   PASS: no magenta; the default picture equals the golden, which is
    //         the picture with debug.depthPrepass off.
    //   FAIL: magenta anywhere, where Equal missed.
    //
    // Every group is one material row with its own color. The layout keeps
    // every group apart, a pair's two members aside, and everything off the
    // floor, so no two groups can tie on a contact line.
    class PrepassEqualSpike: public RenderApp {
        static constexpr u32 PairCount = 6;
        static constexpr u32 DenseSphereCount = 8;
        static constexpr u32 BoxRowCount = 3;
        static constexpr u32 BoxRowLength = 4;
        // z from -5 to 480: from under the camera to the horizon
        static constexpr Vec2 FloorHalf{700.0f, 242.5f};
        // y from -200 to 600, past every edge of the frame
        static constexpr Vec2 WallHalf{700.0f, 400.0f};

        struct FarObject {
            Vec3 position{};
            f32 radius = 1.0f;
            bool box = false;
        };

        GeometryAllocation box{};
        GeometryAllocation sphere{};
        GeometryAllocation denseSphere{};
        GeometryAllocation floorPlane{};
        GeometryAllocation wallPlane{};

    public:
        PrepassEqualSpike()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            const auto boxMesh = MakeBox(0.5f);
            const auto sphereMesh = MakeSphere(1.0f, 48, 24);
            // most of its triangles cover less than a pixel
            const auto denseSphereMesh = MakeSphere(1.0f, 96, 48);
            const auto floorMesh = MakePlane(FloorHalf);
            const auto wallMesh =
                MakePlane(-unitZ(), unitX(), Vec2{WallHalf.x, WallHalf.y});

            box = pool.Add(boxMesh.vertices, boxMesh.indices);
            sphere = pool.Add(sphereMesh.vertices, sphereMesh.indices);
            denseSphere =
                pool.Add(denseSphereMesh.vertices, denseSphereMesh.indices);
            floorPlane = pool.Add(floorMesh.vertices, floorMesh.indices);
            wallPlane = pool.Add(wallMesh.vertices, wallMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            addPairs(scene);
            addDenseSpheres(scene);
            addBoxRows(scene);
            addFarObjects(scene);

            // the floor ends before the wall, and the wall reaches below the
            // floor, so the two never touch
            addPrimitive(
                scene,
                addGroup(scene),
                floorPlane,
                Vec3{FloorHalf.x, 0.0f, FloorHalf.y},
                translateMat({0.0f, 0.0f, 237.5f})
            );
            addPrimitive(
                scene,
                addGroup(scene),
                wallPlane,
                Vec3{WallHalf.x, WallHalf.y, 0.0f},
                translateMat({0.0f, 200.0f, 490.0f})
            );
        }

    private:
        static Config makeConfig() {
            return Config{
                // the shader writes display values: kept as bytes, copied out
                .sceneColorFormat = RHIPixelFormat::RGBA8_UNORM,
                .clearColor = Colors::Magenta,
                .post = {presentPass()},
                .drawCapacity = 64,
                .materialCapacity = 64,
                .shadowMapSize = 0,
                .vertexPoolCapacity = 8192,
                .indexPoolCapacity = 40960
            };
        }

        // the pose the scene was projected from: every object within the
        // frame, and the whole backdrop nearer than the far plane, since a
        // fragment on it passes Equal against the clear but not Less
        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.0f, 2.0f, -6.0f},
                .pitch = 0.06f,
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.1f,
                .farZ = 500.0f
            };
        }

        static MaterialPipelineDesc makePipeline() {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = "Engine/Render/Spike/PrepassEqualSpike.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = "Engine/Render/Spike/PrepassEqualSpike.slang",
                     .entryPoint = "fs_main"},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        // hues from red to blue in golden-ratio steps, never near the
        // magenta clear; brightness alternates between neighbours
        static Vec3 groupColor(u32 group) {
            constexpr auto GoldenRatio = 0.618034f;

            // four of the hue circle's six sextants: red to blue
            const auto hue =
                4.0f * std::fmod(static_cast<f32>(group) * GoldenRatio, 1.0f);
            const auto value = group % 2 == 0 ? 0.9f : 0.6f;
            const auto saturation = group % 3 == 0 ? 0.5f : 0.85f;

            const auto sextant = std::floor(hue);
            const auto f = hue - sextant;
            const auto p = value * (1.0f - saturation);
            const auto q = value * (1.0f - saturation * f);
            const auto t = value * (1.0f - saturation * (1.0f - f));
            switch(static_cast<u32>(sextant)) {
            case 0:
                return {value, t, p};
            case 1:
                return {q, value, p};
            case 2:
                return {p, value, t};
            default:
                return {p, q, value};
            }
        }

        // a box around the origin through any rotation and scale: the world
        // half extents are |R·S| times the local ones
        static AABB3D worldBoundsOf(const Mat4& localToWorld, Vec3 localHalf) {
            const auto reach = [](Vec4 axis, f32 half) {
                return half * Vec3{
                                  std::abs(axis.x),
                                  std::abs(axis.y),
                                  std::abs(axis.z)
                              };
            };

            return AABB3D{
                .center = static_cast<Vec3>(localToWorld[3]),
                .halfScale = reach(localToWorld[0], localHalf.x) +
                             reach(localToWorld[1], localHalf.y) +
                             reach(localToWorld[2], localHalf.z)
            };
        }

        // one material row per group, which only carries its color
        static MaterialHandle addGroup(RenderScene& scene) {
            const auto group = static_cast<u32>(scene.Materials().Count());

            return scene.Materials().Add(
                MaterialResource{
                    .data = MaterialData{.albedo = groupColor(group)},
                    .pipeline = makePipeline()
                }
            );
        }

        static void addPrimitive(
            RenderScene& scene,
            MaterialHandle group,
            const GeometryAllocation& geometry,
            Vec3 localHalf,
            const Mat4& localToWorld
        ) {
            const auto localBounds =
                AABB3D{.center = zeros(), .halfScale = localHalf};
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes = {SubMesh{
                        .geometry = geometry,
                        .localBounds = localBounds
                    }},
                    .materials = {group},
                    .localBounds = localBounds
                }
            );

            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = localToWorld,
                    .worldBounds = worldBoundsOf(localToWorld, localHalf),
                    .mesh = mesh
                }
            );
        }

        void addBox(
            RenderScene& scene,
            MaterialHandle group,
            Vec3 position,
            Vec4 rotation,
            Vec3 scale
        ) const {
            addPrimitive(
                scene,
                group,
                box,
                0.5f * ones(),
                modelMat(position, rotation, scale)
            );
        }

        void addSphere(
            RenderScene& scene,
            MaterialHandle group,
            const GeometryAllocation& geometry,
            Vec3 position,
            f32 radius
        ) const {
            addPrimitive(
                scene,
                group,
                geometry,
                ones(),
                translateMat(position) * scaleMat(radius * ones())
            );
        }

        // box·box, box·sphere, sphere·sphere, twice each; the members of a
        // pair cut into each other and share a group
        void addPairs(RenderScene& scene) const {
            for(u32 i = 0; i < PairCount; ++i) {
                const auto group = addGroup(scene);
                const auto x = (static_cast<f32>(i) - 2.5f) * 1.7f;
                const std::array members = {
                    Vec3{x - 0.18f, 1.0f, 0.0f},
                    Vec3{x + 0.18f, 1.1f, 0.15f}
                };

                for(u32 member = 0; member < members.size(); ++member) {
                    // kind 0 is two boxes, 1 a box and a sphere, 2 two spheres
                    const auto kind = i % 3;
                    const bool isBox = member == 0 ? kind < 2 : kind == 0;
                    if(!isBox) {
                        addSphere(scene, group, sphere, members[member], 0.4f);
                        continue;
                    }

                    const auto seed = static_cast<f32>(2 * i + member);
                    addBox(
                        scene,
                        group,
                        members[member],
                        axisAngle(
                            normalize(Vec3{1.0f, 1.0f - 0.2f * seed, 0.5f}),
                            0.3f + 0.2f * seed
                        ),
                        {0.7f, 0.9f, 0.6f}
                    );
                }
            }
        }

        void addDenseSpheres(RenderScene& scene) const {
            for(u32 i = 0; i < DenseSphereCount; ++i) {
                const Vec3 position{
                    (static_cast<f32>(i) - 3.5f) * 2.3f,
                    i % 2 == 0 ? 3.2f : 3.5f,
                    5.0f
                };

                addSphere(scene, addGroup(scene), denseSphere, position, 0.9f);
            }
        }

        // three rows at 10, 20 and 34 m, rotated and scaled unevenly
        void addBoxRows(RenderScene& scene) const {
            struct BoxRow {
                f32 z = 0.0f;
                f32 spacing = 1.0f;
                // in spacings, so no box hides behind a dense sphere
                f32 shift = 0.0f;
                f32 baseY = 0.0f;
                f32 size = 1.0f;
            };
            constexpr std::array<BoxRow, BoxRowCount> rows = {
                BoxRow{
                    .z = 10.0f,
                    .spacing = 3.2f,
                    .baseY = 1.6f,
                    .size = 0.9f
                },
                BoxRow{
                    .z = 20.0f,
                    .spacing = 5.5f,
                    .shift = 0.4f,
                    .baseY = 2.6f,
                    .size = 1.5f
                },
                BoxRow{
                    .z = 34.0f,
                    .spacing = 9.0f,
                    .shift = 0.5f,
                    .baseY = 4.0f,
                    .size = 2.4f
                }
            };

            for(u32 r = 0; r < BoxRowCount; ++r) {
                const auto& row = rows[r];
                const auto shift = row.shift * row.spacing;

                for(u32 c = 0; c < BoxRowLength; ++c) {
                    const auto k = r * BoxRowLength + c;
                    const auto seed = static_cast<f32>(k);
                    const Vec3 position{
                        (static_cast<f32>(c) - 1.5f) * row.spacing + shift,
                        row.baseY + static_cast<f32>(c % 2) * 0.8f * row.size +
                            static_cast<f32>(r),
                        row.z
                    };
                    const Vec3 scale{
                        row.size * (0.6f + 0.2f * static_cast<f32>(k % 3)),
                        row.size * (1.0f + 0.25f * static_cast<f32>(k % 2)),
                        row.size * (0.4f + 0.15f * static_cast<f32>(k % 4))
                    };

                    addBox(
                        scene,
                        addGroup(scene),
                        position,
                        axisAngle(
                            normalize(Vec3{1.0f, 0.5f + 0.1f * seed, 0.3f}),
                            0.4f + 0.37f * seed
                        ),
                        scale
                    );
                }
            }
        }

        // near the far plane, where D32 depths are coarsest; a box's radius
        // is its half diagonal
        void addFarObjects(RenderScene& scene) const {
            constexpr std::array objects = {
                FarObject{
                    .position = {-230.0f, 60.0f, 330.0f},
                    .radius = 16.0f
                },
                FarObject{
                    .position = {-90.0f, 95.0f, 400.0f},
                    .radius = 24.0f,
                    .box = true
                },
                FarObject{.position = {60.0f, 50.0f, 300.0f}, .radius = 12.0f},
                FarObject{
                    .position = {200.0f, 110.0f, 440.0f},
                    .radius = 24.0f,
                    .box = true
                },
                FarObject{.position = {300.0f, 70.0f, 380.0f}, .radius = 20.0f},
                FarObject{
                    .position = {-330.0f, 120.0f, 430.0f},
                    .radius = 24.0f,
                    .box = true
                }
            };
            constexpr Vec3 BoxShape{1.2f, 1.0f, 0.8f};

            for(u32 i = 0; i < objects.size(); ++i) {
                const auto& object = objects[i];
                const auto group = addGroup(scene);
                if(!object.box) {
                    addSphere(
                        scene,
                        group,
                        sphere,
                        object.position,
                        object.radius
                    );
                    continue;
                }

                const auto size = object.radius / (0.5f * norm(BoxShape));
                addBox(
                    scene,
                    group,
                    object.position,
                    axisAngle(
                        normalize(Vec3{0.3f, 1.0f, 0.2f * static_cast<f32>(i)}),
                        0.5f + 0.3f * static_cast<f32>(i)
                    ),
                    size * BoxShape
                );
            }
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "PrepassEqualSpike",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<PrepassEqualSpike>(argc, argv, windowConfig);
}
