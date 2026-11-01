#include <memory>
#include <vector>

#include "CornellBox.hpp"
#include "Expected.hpp"
#include "LinearAlgebra.hpp"
#include "OrbitCamera.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    // The five walls are one mesh with five submeshes: it is what puts a
    // real submesh list and a real materialSlot indirection under test.
    class CornellRoom: public RenderApp {
        static constexpr f32 BoxHalfSize = 1.0f;
        // the room, three spheres, and the light quad
        static constexpr u32 PrimitiveCount = 5;
        static constexpr u32 WallCount = 5;

        // mirror of the light block in CornellRoom.slang
        struct LightUniforms {
            Vec3 cameraPos{};
            f32 _pad0 = 0.0f;
            Vec3 center{};
            f32 area = 0.0f;
            Vec3 normal{};
            f32 _pad1 = 0.0f;
            Vec3 radiance{};
            f32 ambient = 0.04f;
        };

        using Geometries = std::vector<GeometryAllocation>;

        SceneData source = MakeCornellBox(BoxHalfSize);
        // walls first, then the spheres and the light quad
        Geometries geometry;
        RHIBufferSlice lightCB;
        LightUniforms light{};

    public:
        CornellRoom()
            : RenderApp(
                  makeConfig(),
                  std::make_unique<OrbitCamera>(makeCamera())
              ) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            geometry.reserve(source.objects.size());
            for(const auto& object: source.objects) {
                const auto mesh = bakeTransform(object);

                geometry.push_back(
                    valueOrThrow(pool.Add(mesh.vertices, mesh.indices))
                );
            }
        }

        void ExtractScene(RenderScene& scene) override {
            if(!source.lights.empty()) {
                const auto& source = this->source.lights.front();

                light.center = source.center;
                light.area = 4.0f * source.halfExtent.x * source.halfExtent.y;
                light.normal = source.normal;
                light.radiance = source.radiance;
            }

            MeshResource room{.localBounds = roomBounds()};
            for(u32 i = 0; i < WallCount; ++i) {
                room.subMeshes.push_back(
                    SubMesh{
                        .geometry = geometry[i],
                        .localBounds = roomBounds(),
                        .materialSlot = slotOfWall(room, scene, i)
                    }
                );
            }
            const auto roomMesh = scene.Meshes().Add(room);
            scene.Primitives().Add(
                PrimitiveSnapshot{.worldBounds = roomBounds(), .mesh = roomMesh}
            );

            for(usize i = WallCount; i < source.objects.size(); ++i) {
                const auto& object = source.objects[i];
                const auto bounds = boundsOf(object);
                const auto material = addMaterial(scene, object.material);

                const auto mesh = scene.Meshes().Add(
                    MeshResource{
                        .subMeshes = {SubMesh{
                            .geometry = geometry[i],
                            .localBounds = bounds
                        }},
                        .materials = {material},
                        .localBounds = bounds
                    }
                );
                scene.Primitives().Add(
                    PrimitiveSnapshot{.worldBounds = bounds, .mesh = mesh}
                );
            }
        }

        void OnUpdateFrameData() override {
            light.cameraPos = Camera().Position();
            lightCB = Device().UploadTransient(light);
        }

        void OnBindPass(
            RHICommandList& cmdList,
            const ScenePush& push
        ) override {
            RenderApp::OnBindPass(cmdList, push);
            cmdList.SetGraphicsConstantBuffer(lightCB, LightCBSlot);
        }

    private:
        static constexpr u32 LightCBSlot = 1;

        static Config makeConfig() {
            return Config{
                // the shader writes display values: kept as bytes, copied out
                .sceneColorFormat = RHIPixelFormat::RGBA8_UNORM,
                .post = {presentPass()},
                .drawCapacity = 16,
                .materialCapacity = 16,
                .shadowMapSize = 0,
                .vertexPoolCapacity = 8192,
                .indexPoolCapacity = 32768
            };
        }

        // the pose both capture checks are written against
        static OrbitCamera::Config makeCamera() {
            return OrbitCamera::Config{
                .distance = 4.0f,
                .fovY = radiansOf(60.0f),
                .nearZ = 0.05f,
                .farZ = 100.0f
            };
        }

        static constexpr AABB3D roomBounds() {
            return AABB3D{.center = zeros(), .halfScale = BoxHalfSize * ones()};
        }

        static AABB3D boundsOf(const SceneObject& object) {
            // every generated mesh is centred on the origin before its
            // transform, and only ever translated
            return AABB3D{
                .center = static_cast<Vec3>(object.transform[3]),
                .halfScale = BoxHalfSize * ones()
            };
        }

        // The room's five walls draw with one world matrix,
        // so the per-wall translation has to move into the vertices instead.
        static MeshData bakeTransform(const SceneObject& object) {
            auto mesh = object.mesh;
            const auto offset = static_cast<Vec3>(object.transform[3]);

            for(auto& vertex: mesh.vertices) {
                vertex.position = vertex.position + offset;
            }

            return mesh;
        }

        static MaterialHandle addMaterial(
            RenderScene& scene,
            const Material& source
        ) {
            return scene.Materials().Add(
                MaterialResource{
                    .data =
                        MaterialData{
                            .albedo = source.albedo,
                            .metallic = source.metallic,
                            .emissive = source.emissive,
                            .roughness = source.roughness
                        },
                    .pipeline = opaquePipeline()
                }
            );
        }

        // floor, ceiling and back wall share an albedo: three materials
        // under five submeshes
        u32 slotOfWall(MeshResource& room, RenderScene& scene, u32 wall) {
            const auto& material = source.objects[wall].material;

            for(u32 slot = 0; slot < room.materials.size(); ++slot) {
                if(scene.Materials().GetRef(room.materials[slot]).data.albedo ==
                   material.albedo)
                    return slot;
            }

            room.materials.push_back(addMaterial(scene, material));

            return static_cast<u32>(room.materials.size() - 1);
        }

        static MaterialPipelineDesc opaquePipeline() {
            return MaterialPipelineDesc{
                .vertexShader =
                    {.path = "Engine/Render/Sample/CornellRoom.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader = {
                    .path = "Engine/Render/Sample/CornellRoom.slang",
                    .entryPoint = "fs_main"
                }
            };
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
    return Main<CornellRoom>(argc, argv, windowConfig);
}
