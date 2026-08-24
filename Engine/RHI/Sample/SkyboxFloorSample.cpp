#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include "AppFramework.hpp"
#include "ImageLoader.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "SkyboxFloor.hpp"

namespace Crowy
{
    class SkyboxFloorSample: public App{
    private:
        static constexpr RHIPixelFormat DEPTH_FORMAT = RHIPixelFormat::D32_FLOAT;
        static constexpr f32 FLOOR_HALF_SIZE = 10.0f;
        // 1 maps the grid once across the floor; raise it to tile instead
        static constexpr f32 FLOOR_UV_SCALE = 1.0f;

        RHIGraphicsPipelineStateRAII skyPSO, floorPSO;
        RHITextureRAII depthBuffer;
        // kept so the depth buffer can be rebuilt when the window changes size
        RHIDevice* device = nullptr;

        // the sky is a sphere drawn around the camera, so it is inside out
        RHIBufferRAII skyVertices, skyIndices;
        u32 skyIndexCount = 0;
        RHITextureRAII cubeMap;

        RHIBufferRAII floorVertices, floorIndices;
        u32 floorIndexCount = 0;
        RHITextureRAII floorAlbedo;

        struct SkyPushConstants{
            u64 texture;
        };
        struct FloorPushConstants{
            u64 albedo;
        };

        struct SkyUniforms{
            Mat4 viewProj = unitMat();
        };
        RHIBufferSlice skyCB;

        struct FloorUniforms{
            Mat4 viewProj = unitMat();
            Mat4 model = unitMat();
            Vec3 toLight{};  f32 ambient = 0.25f;
        };
        RHIBufferSlice floorCB;
        Mat4 floorModel = unitMat();

        static constexpr Vec3 toLight{0.4f, 0.8f, -0.45f};

        // a walker standing on the floor, looking around
        Vec3 eye{0.0f, 1.6f, -4.0f};
        Vec3 moveDir = zeros();
        f32 yaw = 0.0f, pitch = 0.0f;

        static constexpr f32 fovY = static_cast<f32>(toRadian(60.0));
        f32 aspect = 1.0f;
        static constexpr f32 nearZ = 0.05f, farZ = 200.0f;

        Vec4 GetCameraRot() const noexcept{
            // yaw in world, then pitch in camera-local frame
            return quat(rotateY(yaw), rotateX(pitch));
        }

        void CreateDepthBuffer(RHIDevice& device, u32 width, u32 height){
            depthBuffer = device.CreateTexture(RHITextureCreateDesc{
                .width = width, .height = height,
                .format = DEPTH_FORMAT,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            });
        }

        RHITextureRAII LoadTexture(RHIDevice& device, const std::filesystem::path& path){
            auto image = LoadImage(path);

            return device.CreateTexture(RHITextureCreateDesc{
                .width = image.width, .height = image.height,
                .mipLevels = image.mipLevels,
                .arraySize = image.arraySize,
                .format = image.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = image.subs
            });
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            // the sky never writes depth, so the floor always wins over it
            // regardless of how the sphere and the plane happen to overlap
            skyPSO = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/CubeMapping.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/CubeMapping.slang",
                    .entryPoint = "fs_main"
                },
                .depthStencil = RHIDepthStencilState{
                    .format = DEPTH_FORMAT,
                    .depthWriteEnable = false,
                    .depthFunc = RHIComparisonFunc::Always
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });

            floorPSO = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/SkyboxFloor.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/SkyboxFloor.slang",
                    .entryPoint = "fs_main"
                },
                .depthStencil = RHIDepthStencilState{
                    .format = DEPTH_FORMAT,
                    .depthWriteEnable = true,
                    .depthFunc = RHIComparisonFunc::Less
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });

            auto scene = MakeSkyboxFloor(FLOOR_HALF_SIZE, FLOOR_UV_SCALE);
            const auto& floor = scene.objects.front();

            floorModel = floor.transform;
            floorVertices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(Vertex) * floor.mesh.vertices.size()),
                .usage = RHIBufferUsage::VertexBuffer,
                .location = RHIMemoryLocation::Device,
                .initialData = floor.mesh.vertices.data()
            });
            floorIndices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(u32) * floor.mesh.indices.size()),
                .usage = RHIBufferUsage::IndexBuffer,
                .location = RHIMemoryLocation::Device,
                .initialData = floor.mesh.indices.data()
            });
            floorIndexCount = static_cast<u32>(floor.mesh.indices.size());
            floorAlbedo = LoadTexture(device, floor.material.albedoMap);

            // a sphere well inside the far plane, wound for a viewer outside it,
            // so it has to be reversed to be seen from within
            auto skyMesh = MakeSphere(50.0f);
            skyVertices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(Vertex) * skyMesh.vertices.size()),
                .usage = RHIBufferUsage::VertexBuffer,
                .location = RHIMemoryLocation::Device,
                .initialData = skyMesh.vertices.data()
            });
            std::reverse(skyMesh.indices.begin(), skyMesh.indices.end());
            skyIndices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(u32) * skyMesh.indices.size()),
                .usage = RHIBufferUsage::IndexBuffer,
                .location = RHIMemoryLocation::Device,
                .initialData = skyMesh.indices.data()
            });
            skyIndexCount = static_cast<u32>(skyMesh.indices.size());

            const auto faces = SkyboxFacePaths(scene.skybox);

            std::vector<RHISubresourceData> subs;
            std::vector<ImageData> faceImages;
            faceImages.reserve(faces.size());
            for(const auto& face: faces){
                faceImages.push_back(LoadImage(face));
            }
            subs.reserve(faces.size() * faceImages.front().subs.size());
            for(const auto& image: faceImages){
                subs.append_range(image.subs);
            }

            const auto& first = faceImages.front();
            cubeMap = device.CreateTexture(RHITextureCreateDesc{
                .width = first.width, .height = first.height,
                .mipLevels = first.mipLevels,
                .arraySize = 6,
                .format = first.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = subs
            });

            aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();
            CreateDepthBuffer(device, swapchain.GetWidth(), swapchain.GetHeight());
        }

        void ProcessInput(const InputProvider& input) override{
            // radian per pixel
            constexpr f32 SENSITIVITY = 0.003f;
            // do not let the camera flip over the poles
            constexpr f32 PITCH_LIMIT = 0.5f * std::numbers::pi_v<f32> - 0.01f;

            if(input.IsKeyDown(MouseButton::RButton)){
                const auto dpos = input.GetMouseDPos();

                yaw += dpos.x * SENSITIVITY;
                pitch = std::clamp(
                    pitch + dpos.y * SENSITIVITY,
                    -PITCH_LIMIT, PITCH_LIMIT
                );
            }

            // move on the ground plane only, so the eye height stays put
            const Vec3 forward{std::sin(yaw), 0.0f, std::cos(yaw)};
            const Vec3 right{std::cos(yaw), 0.0f, -std::sin(yaw)};

            moveDir = zeros();
            if(input.IsKeyDown(KeyCode::W)) moveDir += forward;
            if(input.IsKeyDown(KeyCode::S)) moveDir -= forward;
            if(input.IsKeyDown(KeyCode::D)) moveDir += right;
            if(input.IsKeyDown(KeyCode::A)) moveDir -= right;
        }

        void OnUpdate(f64 deltaTime, f64) override{
            constexpr f32 SPEED = 5.0f;

            if(normSquared(moveDir) > 0.0f){
                eye += SPEED * deltaTime * normalize(moveDir);

                moveDir = zeros();
            }
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            const auto rot = GetCameraRot();
            const auto proj = perspective(fovY, aspect, nearZ, farZ);

            // the sky rotates with the camera but never translates, which is
            // what keeps it infinitely far away
            skyCB = Device().UploadTransient(SkyUniforms{
                .viewProj = proj * viewMat(zeros(), rot)
            });
            floorCB = Device().UploadTransient(FloorUniforms{
                .viewProj = proj * viewMat(eye, rot),
                .model = floorModel,
                .toLight = normalize(toLight)
            });

            std::array colorAttachments = {
                backBuffer
            };
            const std::array acquires{
                AcquireBackBuffer(backBuffer),
                // waits for the previous frame's depth work (WAR), contents
                // discarded - the pass clears anyway
                MakeCrossSubmissionBarrier(
                    *depthBuffer,
                    RHIResourceUsage::DepthWrite,
                    RHIResourceUsage::DepthWrite,
                    /*discardContents=*/true
                )
            };
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
                .depthAttachment = RHIDepthAttachment{
                    .texture = depthBuffer.get(),
                    .loadAction = RHILoadAction::Clear,
                    .storeAction = RHIStoreAction::DontCare,
                    .clearDepthStencil = {.depth = 1.0f}
                }
            }, acquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*skyPSO);
            cmdList.SetVertexBuffer(*skyVertices, 0, sizeof(Vertex));
            cmdList.SetPushGraphicsConstants(SkyPushConstants{
                .texture = cubeMap->GetReadableID(RHITextureViewDesc{
                    .format = cubeMap->GetFormat(),
                    .config = RHITextureViewDesc::TexCube{}
                })
            });
            cmdList.SetGraphicsConstantBuffer(skyCB, 0);
            cmdList.DrawIndexed(
                RHIIndexBufferView{
                    .buffer = skyIndices.get()
                },
                skyIndexCount
            );

            cmdList.SetPipelineState(*floorPSO);
            cmdList.SetVertexBuffer(*floorVertices, 0, sizeof(Vertex));
            cmdList.SetPushGraphicsConstants(FloorPushConstants{
                .albedo = floorAlbedo->GetReadableID()
            });
            cmdList.SetGraphicsConstantBuffer(floorCB, 0);
            cmdList.DrawIndexed(
                RHIIndexBufferView{
                    .buffer = floorIndices.get()
                },
                floorIndexCount
            );

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }

        void OnResize(u32 width, u32 height) override{
            // a minimized window reports zero, which the swapchain also skips
            if(width == 0 || height == 0)
                return;

            aspect = static_cast<f32>(width) / height;
            // the depth buffer has to keep matching the back buffer, so it is
            // thrown away and rebuilt rather than reused
            CreateDepthBuffer(*device, width, height);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "SkyboxFloorSample",
        .width = 1280, .height = 720,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<SkyboxFloorSample>(windowConfig);
}
