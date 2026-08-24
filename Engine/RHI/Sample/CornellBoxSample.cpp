#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <vector>
#include "AppFramework.hpp"
#include "CornellBox.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class CornellBoxSample: public App{
    private:
        static constexpr RHIPixelFormat DEPTH_FORMAT = RHIPixelFormat::D32_FLOAT;
        static constexpr f32 BOX_HALF_SIZE = 1.0f;

        RHIGraphicsPipelineStateRAII pso;
        RHITextureRAII depthBuffer;
        // kept so the depth buffer can be rebuilt when the window changes size
        RHIDevice* device = nullptr;

        // one draw per scene object. the scene never moves, so the per-object
        // constants are filled in once and never touched again
        struct DrawItem{
            RHIBufferRAII vertices, indices;
            u32 indexCount = 0;
            RHIBufferRAII objectCB;
        };
        std::vector<DrawItem> drawItems;

        struct FrameUniforms{
            Mat4 viewProj = unitMat();
            Vec3 cameraPos{};      f32 pad0 = 0.0f;
            Vec3 lightCenter{};    f32 lightArea = 0.0f;
            Vec3 lightNormal{};    f32 pad1 = 0.0f;
            Vec3 lightRadiance{};  f32 ambient = 0.04f;
        };
        RHIBufferSlice frameCB;

        struct ObjectUniforms{
            Mat4 model = unitMat();
            Vec3 albedo{};    f32 metallic = 0.0f;
            Vec3 emissive{};  f32 roughness = 0.5f;
        };

        // the light the shader reads, cached from the scene
        Vec3 lightCenter{}, lightNormal{}, lightRadiance{};
        f32 lightArea = 0.0f;

        // orbit camera, looking at the middle of the room
        f32 yaw = 0.0f, pitch = 0.0f;
        f32 distance = 4.0f;

        static constexpr f32 fovY = static_cast<f32>(toRadian(60.0));
        f32 aspect = 1.0f;
        static constexpr f32 nearZ = 0.05f, farZ = 100.0f;

        Vec3 GetEye() const noexcept{
            const f32 cp = std::cos(pitch);

            // yaw 0 puts the camera on -Z, looking into the open face
            return Vec3{
                 distance * cp * std::sin(yaw),
                 distance * std::sin(pitch),
                -distance * cp * std::cos(yaw)
            };
        }

        void CreateDepthBuffer(RHIDevice& device, u32 width, u32 height){
            depthBuffer = device.CreateTexture(RHITextureCreateDesc{
                .width = width, .height = height,
                .format = DEPTH_FORMAT,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            });
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            this->device = &device;

            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/CornellBox.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/CornellBox.slang",
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

            auto scene = MakeCornellBox(BOX_HALF_SIZE);

            drawItems.reserve(scene.objects.size());
            for(const auto& object: scene.objects){
                const auto& mesh = object.mesh;

                DrawItem item;
                item.vertices = device.CreateBuffer(RHIBufferCreateDesc{
                    .size = static_cast<u32>(sizeof(Vertex) * mesh.vertices.size()),
                    .usage = RHIBufferUsage::VertexBuffer,
                    .location = RHIMemoryLocation::Device,
                    .initialData = mesh.vertices.data()
                });
                item.indices = device.CreateBuffer(RHIBufferCreateDesc{
                    .size = static_cast<u32>(sizeof(u32) * mesh.indices.size()),
                    .usage = RHIBufferUsage::IndexBuffer,
                    .location = RHIMemoryLocation::Device,
                    .initialData = mesh.indices.data()
                });
                item.indexCount = static_cast<u32>(mesh.indices.size());

                ObjectUniforms uniforms{
                    .model = object.transform,
                    .albedo = object.material.albedo,
                    .metallic = object.material.metallic,
                    .emissive = object.material.emissive,
                    .roughness = object.material.roughness
                };
                item.objectCB = device.CreateBuffer(RHIBufferCreateDesc{
                    .size = sizeof(uniforms),
                    .usage = RHIBufferUsage::ConstantBuffer,
                    .location = RHIMemoryLocation::Upload,
                    .cpuAccess = RHICpuAccess::Write,
                    .initialData = &uniforms
                });

                drawItems.push_back(std::move(item));
            }

            if(!scene.lights.empty()){
                const auto& light = scene.lights.front();

                lightCenter = light.center;
                lightNormal = light.normal;
                lightRadiance = light.radiance;
                lightArea = 4.0f * light.halfExtent.x * light.halfExtent.y;
            }

            aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();
            CreateDepthBuffer(device, swapchain.GetWidth(), swapchain.GetHeight());
        }

        void ProcessInput(const InputProvider& input) override{
            if(!input.IsKeyDown(MouseButton::RButton))
                return;

            // radian per pixel
            constexpr f32 SENSITIVITY = 0.005f;
            // do not let the camera flip over the poles
            constexpr f32 PITCH_LIMIT = 0.5f * std::numbers::pi_v<f32> - 0.01f;

            const auto dpos = input.GetMouseDPos();

            yaw -= dpos.x * SENSITIVITY;
            pitch = std::clamp(
                pitch + dpos.y * SENSITIVITY,
                -PITCH_LIMIT, PITCH_LIMIT
            );
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            const auto eye = GetEye();
            const auto view = lookAt(eye, zeros(), Vec3{0.0f, 1.0f, 0.0f});
            const auto proj = perspective(fovY, aspect, nearZ, farZ);

            frameCB = Device().UploadTransient(FrameUniforms{
                .viewProj = proj * view,
                .cameraPos = eye,
                .lightCenter = lightCenter,
                .lightArea = lightArea,
                .lightNormal = lightNormal,
                .lightRadiance = lightRadiance
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

            cmdList.SetPipelineState(*pso);
            cmdList.SetGraphicsConstantBuffer(frameCB, 0);

            for(const auto& item: drawItems){
                cmdList.SetVertexBuffer(*item.vertices, 0, sizeof(Vertex));
                cmdList.SetGraphicsConstantBuffer(*item.objectCB, 1);
                cmdList.DrawIndexed(
                    RHIIndexBufferView{
                        .buffer = item.indices.get()
                    },
                    item.indexCount
                );
            }

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
        .title = "CornellBoxSample",
        .width = 800, .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<CornellBoxSample>(windowConfig);
}
