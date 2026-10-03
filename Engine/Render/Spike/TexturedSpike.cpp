#include <array>
#include <cmath>
#include <memory>
#include <numbers>
#include <vector>

#include "FlyCamera.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "RHIDevice.hpp"
#include "RenderApp.hpp"

namespace Crowy
{
    // Regression check for textures through the standard pipeline, with
    // textures written by code so it needs no content:
    //
    //   - a box sampling a 4 x 4 palette with the nearest sampler: sixteen
    //     flat cells per face, hard edges, no blend between cells
    //   - a quad sampling the middle quarter of a 256 x 256 gradient grid
    //     through the material's UV scale and offset, linearly filtered
    //   - a black quad whose emission is one palette cell
    //   - two floor tiles in a floating plate's shadow: the left receives it and
    //     goes dark, the right refuses it (MaterialFlags::NoShadowReceive)
    //     and stays as lit as the floor in the sun
    //   - a translucent card whose texel alpha steps 1/4, 1/2, 3/4, 1: four
    //     bands of red over the floor, the last one solid
    //
    //   PASS: the picture equals the golden.
    class TexturedSpike: public RenderApp {
        static constexpr u32 PaletteSize = 4;
        static constexpr u32 GridSize = 256;
        static constexpr u32 StripSize = 4;

        struct Texel {
            u8 r = 0;
            u8 g = 0;
            u8 b = 0;
            u8 a = 255;
        };

        GeometryAllocation box{};
        GeometryAllocation floorPlane{};
        GeometryAllocation tile{};
        // facing the camera, u to the right and v down
        GeometryAllocation card{};

    public:
        TexturedSpike()
            : RenderApp(makeConfig(), std::make_unique<FlyCamera>(makeCamera())) {}

    protected:
        void OnBuildGeometry(GeometryPool& pool) override {
            const auto boxMesh = MakeBox(0.5f);
            const auto floorMesh = MakePlane(Vec2{4.0f, 4.0f});
            const auto tileMesh = MakePlane(Vec2{0.25f, 0.25f});
            const auto cardMesh = MakePlane(-unitZ(), unitX(), 0.5f);

            box = pool.Add(boxMesh.vertices, boxMesh.indices);
            floorPlane = pool.Add(floorMesh.vertices, floorMesh.indices);
            tile = pool.Add(tileMesh.vertices, tileMesh.indices);
            card = pool.Add(cardMesh.vertices, cardMesh.indices);
        }

        void ExtractScene(RenderScene& scene) override {
            // down and away from the camera, so shadows fall behind their
            // casters where the camera sees them
            scene.Lights().Add(
                LightSnapshot{
                    .castShadow = true,
                    .color = ones(),
                    .intensity = 3.0f,
                    .direction = normalize(Vec3{0.0f, -1.0f, 0.6f})
                }
            );
            scene.Environment() = EnvironmentSnapshot{
                .skyAmbient = {0.15f, 0.18f, 0.24f},
                .groundAmbient = {0.04f, 0.03f, 0.02f}
            };

            const auto palette = scene.Textures().Add(
                TextureResource{
                    .texture = makeTexture(paletteTexels(), PaletteSize),
                    .sampler = TextureSampler::NearestClamp
                }
            );
            const auto grid = scene.Textures().Add(
                TextureResource{
                    .texture = makeTexture(gridTexels(), GridSize),
                    .sampler = TextureSampler::LinearClamp
                }
            );
            const auto strip = scene.Textures().Add(
                TextureResource{
                    .texture = makeTexture(stripTexels(), StripSize, 1),
                    .sampler = TextureSampler::NearestClamp
                }
            );

            const auto plain = addMaterial(scene, MaterialResource{
                .data = {.albedo = {0.6f, 0.6f, 0.6f}, .roughness = 0.9f},
                .pipeline = opaquePipeline()
            });
            add(scene, plain, floorPlane, {0.0f, 0.0f, 2.0f}, {4.0f, 0.001f, 4.0f});

            const auto paletteBox = addMaterial(scene, MaterialResource{
                .data = {.roughness = 0.9f},
                .pipeline = opaquePipeline(),
                .maps = {.albedo = palette}
            });
            add(scene, paletteBox, box, {-1.6f, 0.5f, 1.0f}, ones() * 0.5f, 1.0f);

            // the grid's middle quarter
            const auto atlas = addMaterial(scene, MaterialResource{
                .data = {
                    .roughness = 0.9f,
                    .uvScaleOffset = {0.5f, 0.5f, 0.25f, 0.25f}
                },
                .pipeline = opaquePipeline(),
                .maps = {.albedo = grid}
            });
            add(scene, atlas, card, {1.5f, 1.2f, 1.6f}, {0.5f, 0.5f, 0.0f});

            // the palette's cell (1, 2), lit by nothing but itself
            const auto glow = addMaterial(scene, MaterialResource{
                .data = {
                    .albedo = zeros(),
                    .emissive = ones(),
                    .uvScaleOffset = {0.0f, 0.0f, 1.25f / PaletteSize, 2.25f / PaletteSize}
                },
                .pipeline = opaquePipeline(),
                .maps = {.emissive = palette}
            });
            add(scene, glow, card, {0.0f, 1.55f, 2.6f}, {0.5f, 0.5f, 0.0f}, 0.6f);

            // the plate whose shadow covers both tiles, high enough for the
            // camera to look under it
            add(scene, plain, box, {0.0f, 1.2f, 1.0f}, {0.5f, 0.5f, 0.5f}, 1.0f, {1.6f, 0.05f, 0.8f});

            const auto receives = addMaterial(scene, MaterialResource{
                .data = {.albedo = {0.9f, 0.85f, 0.7f}, .roughness = 0.9f},
                .pipeline = opaquePipeline()
            });
            add(scene, receives, tile, {-0.35f, 0.002f, 1.7f}, {0.25f, 0.001f, 0.25f});

            const auto refuses = addMaterial(scene, MaterialResource{
                .data = {
                    .albedo = {0.9f, 0.85f, 0.7f},
                    .roughness = 0.9f,
                    .flags = static_cast<u32>(MaterialFlags::NoShadowReceive)
                },
                .pipeline = opaquePipeline()
            });
            add(scene, refuses, tile, {0.35f, 0.002f, 1.7f}, {0.25f, 0.001f, 0.25f});

            const auto veil = addMaterial(scene, MaterialResource{
                .data = {.roughness = 0.9f},
                .pipeline = translucentPipeline(),
                .maps = {.albedo = strip}
            });
            add(scene, veil, card, {0.7f, 0.3f, 0.4f}, {0.5f, 0.5f, 0.0f}, 0.5f);
        }

    private:
        static Config makeConfig() {
            return Config{
                .clearColor = {0.12f, 0.16f, 0.24f, 1.0f},
                .drawCapacity = 32,
                .materialCapacity = 16,
                .shadowMapSize = 1024,
                .vertexPoolCapacity = 1024,
                .indexPoolCapacity = 1024
            };
        }

        static FlyCamera::Config makeCamera() {
            return FlyCamera::Config{
                .position = {0.0f, 1.0f, -1.5f},
                .pitch = 0.25f,
                .fovY = std::numbers::pi_v<f32> / 3,
                .nearZ = 0.05f,
                .farZ = 50.0f
            };
        }

        static MaterialPipelineDesc opaquePipeline() {
            constexpr CStr StandardForward = "Engine/Render/Shader/StandardForward.slang";

            return MaterialPipelineDesc{
                .vertexShader = {.path = StandardForward, .entryPoint = "vs_main"},
                .fragmentShader = {.path = StandardForward, .entryPoint = "fs_opaque"},
                .rasterizer = {.frontCounterClockwise = false},
                .profile = "sm_6_8"
            };
        }

        // blended over the opaque scene by the texel's alpha
        static MaterialPipelineDesc translucentPipeline() {
            auto pipeline = opaquePipeline();
            pipeline.fragmentShader.entryPoint = "fs_translucent";
            pipeline.domain = MaterialDomain::Translucent;

            RHIBlendState blend{};
            blend.renderTargets[0] = RHIRenderTargetBlendState{
                .blendEnable = true,
                .srcBlend = RHIBlend::SrcAlpha,
                .dstBlend = RHIBlend::InvSrcAlpha
            };
            pipeline.blend = blend;

            return pipeline;
        }

        // sixteen colors far apart, row-major from the top-left
        static std::vector<Texel> paletteTexels() {
            static constexpr std::array<Texel, PaletteSize * PaletteSize> Colors{{
                {230, 25, 75}, {60, 180, 75}, {255, 225, 25}, {0, 130, 200},
                {245, 130, 48}, {145, 30, 180}, {70, 240, 240}, {240, 50, 230},
                {210, 245, 60}, {250, 190, 212}, {0, 128, 128}, {220, 190, 255},
                {170, 110, 40}, {255, 250, 200}, {128, 0, 0}, {255, 255, 255},
            }};

            return {Colors.begin(), Colors.end()};
        }

        // red across, green down, a darker checker of 32-texel squares
        static std::vector<Texel> gridTexels() {
            constexpr u32 GridCell = 32;

            std::vector<Texel> texels(GridSize * GridSize);
            for(u32 y = 0; y < GridSize; ++y) {
                for(u32 x = 0; x < GridSize; ++x) {
                    const bool dark = ((x / GridCell) + (y / GridCell)) % 2 == 1;
                    const auto shade = dark ? 0.5f : 1.0f;
                    texels[y * GridSize + x] = Texel{
                        .r = static_cast<u8>(shade * static_cast<f32>(x)),
                        .g = static_cast<u8>(shade * static_cast<f32>(y)),
                        .b = static_cast<u8>(shade * 160.0f)
                    };
                }
            }

            return texels;
        }

        // one red, its alpha stepping a quarter at a time
        static std::vector<Texel> stripTexels() {
            std::vector<Texel> texels;
            for(u32 i = 1; i <= StripSize; ++i)
                texels.push_back(Texel{.r = 230, .g = 25, .b = 75, .a = static_cast<u8>(i * 255 / StripSize)});

            return texels;
        }

        static MaterialHandle addMaterial(RenderScene& scene, MaterialResource material) {
            return scene.Materials().Add(std::move(material));
        }

        // a mesh of one submesh at `position`, its local half extents
        // stretched by `scale`; `size` scales a unit mesh before that
        static void add(
            RenderScene& scene,
            MaterialHandle material,
            const GeometryAllocation& geometry,
            Vec3 position,
            Vec3 localHalf,
            f32 size = 1.0f,
            Vec3 scale = ones()
        ) {
            const AABB3D local{.center = zeros(), .halfScale = localHalf};
            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .subMeshes = {SubMesh{.geometry = geometry, .localBounds = local}},
                    .materials = {material},
                    .localBounds = local
                }
            );
            const auto stretch = size * scale;
            scene.Primitives().Add(
                PrimitiveSnapshot{
                    .localToWorld = translateMat(position) * scaleMat(stretch),
                    .worldBounds = AABB3D{.center = position, .halfScale = localHalf * stretch},
                    .mesh = mesh
                }
            );
        }

        RHITextureRAII makeTexture(const std::vector<Texel>& texels, u32 width, u32 height = 0) {
            const std::array initial{RHISubresourceData{
                .data = texels.data(),
                .rowPitch = width * sizeof(Texel)
            }};

            return Device().CreateTexture(
                RHITextureCreateDesc{
                    .width = width,
                    .height = height == 0 ? width : height,
                    .format = RHIPixelFormat::RGBA8_UNORM_SRGB,
                    .usage = RHITextureUsage::ShaderRead,
                    .initialData = initial
                },
                "TexturedSpike"
            );
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "TexturedSpike",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<TexturedSpike>(argc, argv, windowConfig);
}
