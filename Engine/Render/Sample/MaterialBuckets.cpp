#include <array>
#include <memory>
#include <numbers>

#include "FlyCamera.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    // Bucket order is discovery order, which is primitive order,
    // so the opaque primitives are registered first.
    // There is no sort key yet; the moment a second pass or
    // a real translucent scene arrives, there has to be one.
    class MaterialBuckets: public RenderApp {
        static constexpr f32 Radius = 0.5f;
        static constexpr Color SkyColor{0.10f, 0.11f, 0.14f, 1.0f};

        static constexpr u32 SphereCount = 3;
        static constexpr u32 PaneCount = 3;

        GeometryAllocation sphere{};
        // faces away from the camera, so a back-face-culled pipeline drops it
        GeometryAllocation pane{};
        GeometryAllocation glass{};

    public:
        MaterialBuckets()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(
            RHICommandList& cmdList,
            GeometryPool& pool
        ) override {
            const auto sphereMesh = MakeSphere(Radius, 32, 16);
            const auto paneMesh = MakePlane(unitZ(), unitX(), 1.0f);
            const auto glassMesh = MakePlane(-unitZ(), unitX(), 1.0f);

            sphere = pool.Add(cmdList, sphereMesh.vertices, sphereMesh.indices);
            pane = pool.Add(cmdList, paneMesh.vertices, paneMesh.indices);
            glass = pool.Add(cmdList, glassMesh.vertices, glassMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            const auto opaque =
                addMaterial(scene, {0.85f, 0.55f, 0.30f}, opaquePipeline());
            const auto doubleSided = addMaterial(
                scene,
                {0.35f, 0.70f, 0.45f},
                doubleSidedPipeline()
            );
            const auto translucent = addMaterial(
                scene,
                {0.35f, 0.55f, 0.95f},
                translucentPipeline()
            );

            const auto sphereMesh = addMesh(scene, sphere, opaque, Radius);
            const auto paneMesh = addMesh(scene, pane, doubleSided, 1.0f);
            const auto glassMesh = addMesh(scene, glass, translucent, 1.0f);

            // opaque first, so their bucket is discovered first
            for(u32 i = 0; i < SphereCount; ++i) {
                const auto x = (static_cast<f32>(i) - 1.0f) * 1.6f;

                addPrimitive(scene, sphereMesh, {x, 0.0f, 0.0f}, Radius);
            }
            // its front face points away, so this only appears at all because
            // its pipeline culls nothing
            for(u32 i = 0; i < PaneCount; ++i) {
                const auto x = (static_cast<f32>(i) - 1.0f) * 1.6f;

                addPrimitive(scene, paneMesh, {x, 1.8f, 1.5f}, 1.0f);
            }
            for(u32 i = 0; i < PaneCount; ++i) {
                const auto x = (static_cast<f32>(i) - 1.0f) * 1.6f;

                addPrimitive(scene, glassMesh, {x, 0.0f, -1.2f}, 1.0f);
            }
        }

    private:
        static Config makeConfig() {
            return Config{
                .clearColor = SkyColor,
                .drawCapacity = 32,
                .materialCapacity = 8,
                .vertexPoolCapacity = 4096,
                .indexPoolCapacity = 16384
            };
        }

        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.0f, 0.6f, -5.0f},
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 50.0f
            };
        }

        static MaterialHandle addMaterial(
            RenderScene& scene,
            Vec3 albedo,
            const MaterialPipelineDesc& pipeline
        ) {
            return scene.Materials().Add(
                MaterialResource{
                    .data = MaterialData{.albedo = albedo},
                    .pipeline = pipeline
                }
            );
        }

        static MeshHandle addMesh(
            RenderScene& scene,
            const GeometryAllocation& geometry,
            MaterialHandle material,
            f32 halfSize
        ) {
            const auto bounds =
                AABB3D{.center = zeros(), .halfScale = halfSize * ones()};

            return scene.Meshes().Add(
                MeshResource{
                    .subMeshes =
                        {SubMesh{.geometry = geometry, .localBounds = bounds}},
                    .materials = {material},
                    .localBounds = bounds
                }
            );
        }

        static void addPrimitive(
            RenderScene& scene,
            MeshHandle mesh,
            Vec3 position,
            f32 halfSize
        ) {
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = translateMat(position),
                    .worldBounds =
                        AABB3D{
                            .center = position,
                            .halfScale = halfSize * ones()
                        },
                    .mesh = mesh
                }
            );
        }

        static MaterialPipelineDesc basePipeline(CStr fragmentEntry) {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = "Engine/Shader/MaterialBuckets.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = "Engine/Shader/MaterialBuckets.slang",
                     .entryPoint = fragmentEntry},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        static MaterialPipelineDesc opaquePipeline() {
            return basePipeline("fs_opaque");
        }

        static MaterialPipelineDesc doubleSidedPipeline() {
            auto pipeline = basePipeline("fs_opaque");
            pipeline.rasterizer.cullMode = RHICullMode::None;

            return pipeline;
        }

        static MaterialPipelineDesc translucentPipeline() {
            auto pipeline = basePipeline("fs_translucent");
            pipeline.depthWrite = false;

            RHIBlendState blend{};
            blend.renderTargets[0] = RHIRenderTargetBlendState{
                .blendEnable = true,
                .srcBlend = RHIBlend::SrcAlpha,
                .dstBlend = RHIBlend::InvSrcAlpha
            };
            pipeline.blend = blend;

            return pipeline;
        }
    };
}

int main(void) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "MaterialBuckets",
        .width = 800,
        .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<MaterialBuckets>(windowConfig);
}
