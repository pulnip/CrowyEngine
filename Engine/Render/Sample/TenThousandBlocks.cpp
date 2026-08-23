#include <array>
#include <numbers>

#include "IntMath.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "Primitives.hpp"
#include "RHIPipelineState.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    // Engine/RHI/Sample/TenThousandCubes built on RenderScene instead of a
    // hand-rolled object list. Same lattice, meshes, pose and clear color,
    // so a smoke capture of the two has to compare equal.
    class TenThousandBlocks: public RenderApp {
        // keep in sync with TenThousandBlocks.slang
        static constexpr u32 GridX = 20, GridY = 20, GridZ = 25;
        static constexpr u32 BlockCount = GridX * GridY * GridZ;
        static constexpr f32 BlockSpacing = 5.0f;
        static constexpr f32 BlockHalfSize = 0.5f;

        static constexpr u32 MeshTypeCount = 3;
        using Geometries = std::array<GeometryAllocation, MeshTypeCount>;
        using Meshes = std::array<MeshHandle, MeshTypeCount>;

        static constexpr Color SkyColor{0.55f, 0.78f, 0.95f, 1.0f};

        RHIGraphicsPipelineStateRAII pso;
        Geometries geometry;
        Meshes meshes;
        MaterialHandle material;

    public:
        TenThousandBlocks()
            : RenderApp(makeConfig()) {}

    protected:
        void OnCreatePipelines(
            RHIDevice& device,
            RHIPixelFormat colorFormat,
            RHIPixelFormat depthFormat
        ) override {
            pso = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .preRasterizer =
                        RHILegacyFrontendDesc{
                            .vertexLayout = VERTEX_INPUT_LAYOUT,
                            .topology = RHIPrimitiveTopology::TriangleList,
                            .vertexShader =
                                {.path =
                                     "Engine/Shader/TenThousandBlocks.slang",
                                 .entryPoint = "vs_main"}
                        },
                    .rasterizer =
                        RHIRasterizerState{.frontCounterClockwise = false},
                    .fragmentShader =
                        {.path = "Engine/Shader/TenThousandBlocks.slang",
                         .entryPoint = "fs_main"},
                    .depthStencil =
                        RHIDepthStencilState{
                            .format = depthFormat,
                            .depthWriteEnable = true,
                            .depthFunc = RHIComparisonFunc::Less
                        },
                    .renderTargetFormats = {colorFormat},
                    .renderTargetCount = 1,
                    // SV_StartInstanceLocation carries the draw ID
                    .profile = "sm_6_8"
                }
            );
        }

        void OnBuildGeometry(
            RHICommandList& cmdList,
            GeometryPool& pool
        ) override {
            const auto boxMesh = MakeBox(BlockHalfSize);
            const auto sphereMesh = MakeSphere(BlockHalfSize, 16, 8);
            // single-sided quad facing the start camera
            const auto planeMesh = MakePlane(-unitZ(), unitX(), BlockHalfSize);

            geometry[0] = pool.Add(cmdList, boxMesh.vertices, boxMesh.indices);
            geometry[1] =
                pool.Add(cmdList, sphereMesh.vertices, sphereMesh.indices);
            geometry[2] =
                pool.Add(cmdList, planeMesh.vertices, planeMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            // the shader colours by objectID and ignores the material,
            // so one row covers the whole lattice
            material = scene.Materials().Add(MaterialResource{});

            const auto localBounds =
                AABB3D{.center = zeros(), .halfScale = BlockHalfSize * ones()};
            for(u32 i = 0; i < MeshTypeCount; ++i) {
                meshes[i] = scene.Meshes().Add(
                    MeshResource{
                        .subMeshes = {SubMesh{
                            .geometry = geometry[i],
                            .localBounds = localBounds
                        }},
                        .materials = {material},
                        .localBounds = localBounds
                    }
                );
            }

            for(u32 i = 0; i < BlockCount; ++i) {
                const auto position = blockPosition(i);

                scene.Primitives().Add(
                    PrimitiveSnapshot{
                        .localToWorld = translateMat(position),
                        // every mesh type fits the box's extent
                        .worldBounds =
                            AABB3D{
                                .center = position,
                                .halfScale = BlockHalfSize * ones()
                            },
                        .mesh = meshes[i % MeshTypeCount]
                    }
                );
            }
        }

        RHIGraphicsPipelineState& Pipeline() override { return *pso; }

    private:
        static Config makeConfig() {
            return Config{
                .clearColor = SkyColor,
                .drawCapacity = BlockCount,
                .vertexPoolCapacity = 1024,
                .indexPoolCapacity = 4096,
                .camera = FlyCamera::Config{
                    .position = {50.0f, 50.0f, 37.5f},
                    .fovY = std::numbers::pi_v<f32> / 3,
                    .nearZ = 0.1f,
                    .farZ = 300.0f
                }
            };
        }

        static constexpr Vec3 blockPosition(u32 index) {
            return Vec3{
                       static_cast<f32>(index % GridX),
                       static_cast<f32>(floorDiv(index, GridX) % GridY),
                       static_cast<f32>(floorDiv(index, GridX * GridY))
                   } *
                   BlockSpacing;
        }
    };
}

int main(void) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "TenThousandBlocks",
        .width = 800,
        .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<TenThousandBlocks>(windowConfig);
}
