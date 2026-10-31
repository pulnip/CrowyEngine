#include "TenThousandBlocks.h"

#include <array>
#include <memory>
#include <numbers>

#include "Expected.hpp"
#include "FlyCamera.hpp"
#include "IntMath.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "MeshRowComponent.hpp"
#include "Primitives.hpp"
#include "RHIPipelineState.hpp"
#include "RenderApp.hpp"
#include "World.hpp"

namespace Crowy
{
    class TenThousandBlocks: public RenderApp {
        static constexpr u32 GridX = BLOCK_GRID_X;
        static constexpr u32 GridY = BLOCK_GRID_Y;
        static constexpr u32 GridZ = BLOCK_GRID_Z;
        static constexpr u32 BlockCount = GridX * GridY * GridZ;
        static constexpr f32 BlockSpacing = 5.0f;
        static constexpr f32 BlockHalfSize = 0.5f;

        static constexpr u32 MeshTypeCount = 3;
        using Geometries = std::array<GeometryAllocation, MeshTypeCount>;
        using Meshes = std::array<MeshHandle, MeshTypeCount>;

        static constexpr Color SkyColor{0.55f, 0.78f, 0.95f, 1.0f};

        Geometries geometry;
        Meshes meshes;
        MaterialHandle material;
        World world;

    public:
        TenThousandBlocks()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<FlyCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            const auto boxMesh = MakeBox(BlockHalfSize);
            const auto sphereMesh = MakeSphere(BlockHalfSize, 16, 8);
            // single-sided quad facing the start camera
            const auto planeMesh = MakePlane(-unitZ(), unitX(), BlockHalfSize);

            geometry[0] =
                valueOrThrow(pool.Add(boxMesh.vertices, boxMesh.indices));
            geometry[1] =
                valueOrThrow(pool.Add(sphereMesh.vertices, sphereMesh.indices));
            geometry[2] =
                valueOrThrow(pool.Add(planeMesh.vertices, planeMesh.indices));
        }

        void ExtractScene(RenderScene& scene) override {
            // the shader colors by objectID and ignores the material,
            // so one row covers the whole lattice
            material = scene.Materials().Add(
                MaterialResource{
                    .pipeline = MaterialPipelineDesc{
                        .vertexShader =
                            {.path =
                                 "Engine/Render/Sample/TenThousandBlocks.slang",
                             .entryPoint = "vs_main"},
                        .fragmentShader = {
                            .path =
                                "Engine/Render/Sample/TenThousandBlocks.slang",
                            .entryPoint = "fs_main"
                        }
                    }
                }
            );

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

            // the shader colors by row, so the rows land in lattice order
            world.Provide(scene);
            for(u32 i = 0; i < BlockCount; ++i) {
                world.Add(
                    world.Spawn(Transform{.position = blockPosition(i)}),
                    MeshRowComponent{meshes[i % MeshTypeCount]}
                );
            }
        }

        void OnUpdateScene(f64 seconds) override { world.Frame(seconds); }

    private:
        static Config makeConfig() {
            return Config{
                // the shader writes display values: kept as bytes, copied out
                .sceneColorFormat = RHIPixelFormat::RGBA8_UNORM,
                .clearColor = SkyColor,
                .post = {presentPass()},
                .drawCapacity = BlockCount,
                .shadowMapSize = 0,
                .vertexPoolCapacity = 1024,
                .indexPoolCapacity = 4096
            };
        }

        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {50.0f, 50.0f, 37.5f},
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.1f,
                .farZ = 300.0f
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

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .width = 800,
        .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .resizable = true,
    };
    return Main<TenThousandBlocks>(argc, argv, windowConfig);
}
