#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <numbers>

#include "Expected.hpp"
#include "FlyCamera.hpp"
#include "LinearAlgebra.hpp"
#include "MeshData.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    class VertexPullSpike: public RenderApp {
        // keep in sync with VertexPullSpike.slang
        static constexpr u32 SlotCount = 4;
        static constexpr u32 FirstSideCount = 3;

        static constexpr f32 Radius = 0.6f;
        static constexpr f32 SlotSpacing = 1.5f;
        static constexpr Color SkyColor{0.10f, 0.11f, 0.14f, 1.0f};

        using Geometries = std::array<GeometryAllocation, SlotCount>;

        Geometries polygons{};

    public:
        VertexPullSpike()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            for(u32 i = 0; i < SlotCount; ++i) {
                const auto mesh = makePolygon(FirstSideCount + i);

                polygons[i] =
                    valueOrThrow(pool.Add(mesh.vertices, mesh.indices));
            }
        }

        void ExtractScene(RenderScene& scene) override {
            // the shader colors by objectID, so one row covers every slot
            const auto material = scene.Materials().Add(
                MaterialResource{.pipeline = makePipeline()}
            );

            const auto bounds =
                AABB3D{.center = zeros(), .halfScale = Radius * ones()};
            for(u32 i = 0; i < SlotCount; ++i) {
                const auto mesh = scene.Meshes().Add(
                    MeshResource{
                        .subMeshes = {SubMesh{
                            .geometry = polygons[i],
                            .localBounds = bounds
                        }},
                        .materials = {material},
                        .localBounds = bounds
                    }
                );

                const auto position = slotPosition(i);
                scene.Primitives().Add(
                    PrimitiveSnapshot{
                        .localToWorld = translateMat(position),
                        .worldBounds =
                            AABB3D{
                                .center = position,
                                .halfScale = Radius * ones()
                            },
                        .mesh = mesh
                    }
                );
            }
        }

        // a fullscreen pass between opaque and translucent that reads depth and
        // blends alpha 0 must leave the same bytes (FullscreenProof.slang)
        FrameGraphDesc DescribePipeline(
            const StandardFrameConfig& config
        ) override {
            constexpr FrameTargetID SceneDepth = 1;
            constexpr CStr Proof = "Engine/Render/Spike/FullscreenProof.slang";

            auto desc = describeStandardFrame(config);

            // every destination channel kept, alpha included
            RHIBlendState keep{};
            keep.renderTargets[0] = RHIRenderTargetBlendState{
                .blendEnable = true,
                .srcBlend = RHIBlend::SrcAlpha,
                .dstBlend = RHIBlend::InvSrcAlpha,
                .srcBlendAlpha = RHIBlend::Zero,
                .dstBlendAlpha = RHIBlend::One
            };
            const auto translucent = std::ranges::find(
                desc.passes,
                Str{"Translucent"},
                &PassDesc::name
            );
            desc.passes.insert(
                translucent,
                PassDesc{
                    .name = "Blend",
                    .colors = {ColorTargetUse{
                        .target = desc.sceneColor,
                        .load = RHILoadAction::Load
                    }},
                    .reads = {SceneDepth},
                    .kind = FullscreenPassDesc{
                        .fragmentShader =
                            {.path = Proof, .entryPoint = "fs_blend"},
                        .blend = keep
                    }
                }
            );

            return desc;
        }

    private:
        static Config makeConfig() {
            return Config{
                // the shader writes display values: kept as bytes, copied out
                .sceneColorFormat = RHIPixelFormat::RGBA8_UNORM,
                .clearColor = SkyColor,
                .post = {presentPass()},
                .drawCapacity = SlotCount,
                .materialCapacity = 1,
                .shadowMapSize = 0,
                .vertexPoolCapacity = 1024,
                .indexPoolCapacity = 4096
            };
        }

        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.0f, 0.0f, -6.0f},
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 50.0f
            };
        }

        // nothing here depends on winding, and a spike that can fail by
        // facing the wrong way is a worse spike
        static MaterialPipelineDesc makePipeline() {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = "Engine/Render/Spike/VertexPullSpike.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader =
                    {.path = "Engine/Render/Spike/VertexPullSpike.slang",
                     .entryPoint = "fs_main"},
                .rasterizer = {.cullMode = RHICullMode::None}
            };
        }

        // A regular polygon facing the camera, as a fan from its first vertex,
        // so the vertex count is the side count exactly.
        static MeshData makePolygon(u32 sideCount) {
            constexpr auto Turn = 2 * std::numbers::pi_v<f32>;

            MeshData mesh;

            mesh.vertices.reserve(sideCount);
            for(u32 i = 0; i < sideCount; ++i) {
                const auto angle =
                    Turn * static_cast<f32>(i) / static_cast<f32>(sideCount);

                mesh.vertices.push_back(
                    Vertex{
                        .position =
                            {Radius * std::sin(angle),
                             Radius * std::cos(angle),
                             0.0f},
                        .normal = -unitZ()
                    }
                );
            }

            mesh.indices.reserve(3 * (sideCount - 2));
            for(u32 i = 1; i + 1 < sideCount; ++i) {
                mesh.indices.push_back(0);
                mesh.indices.push_back(i);
                mesh.indices.push_back(i + 1);
            }

            return mesh;
        }

        static constexpr Vec3 slotPosition(u32 slot) {
            const auto offset = static_cast<f32>(slot) - (SlotCount - 1) * 0.5f;

            return Vec3{offset * SlotSpacing, 0.0f, 0.0f};
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "VertexPullSpike",
        .width = 800,
        .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<VertexPullSpike>(argc, argv, windowConfig);
}
