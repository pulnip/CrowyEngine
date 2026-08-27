#include <algorithm>
#include <array>
#include <numbers>
#include "AppFramework.hpp"
#include "ImageLoader.hpp"
#include "InputProvider.hpp"
#include "LinearAlgebra.hpp"
#include "MeshGenerator.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class CubeMapping: public App{
    private:
        RHIGraphicsPipelineStateRAII pso;

        RHIBufferRAII vertices, indices;
        u32 indexCount = 0;
        RHITextureRAII cubeMap;

        struct PushConstants{
            u64 texture;
        };

        // camera geometry
        f32 yaw = 0.0f, pitch = 0.0f;
        Vec4 GetCameraRot() const noexcept{
            // yaw in world, then pitch in camera-local frame
            return quat(rotateY(yaw), rotateX(pitch));
        }

        // camera lens
        static constexpr f32 fovY = static_cast<f32>(toRadian(60.0));
        f32 aspect;
        static constexpr f32 nearZ = 0.1f, farZ = 100.0f;

        struct Uniforms{
            Mat4 viewProj = unitMat();
        };

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .vertexLayout = VERTEX_INPUT_LAYOUT,
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/RHI/Sample/CubeMapping.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/RHI/Sample/CubeMapping.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });

            auto cubeMesh = MakeSphere(10.0f);
            vertices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(Vertex) * cubeMesh.vertices.size()),
                .initialData = cubeMesh.vertices.data()
            });
            // MakeSphere winds its faces for a viewer outside the mesh,
            // but a skybox is seen from the inside, so flip the winding
            std::reverse(cubeMesh.indices.begin(), cubeMesh.indices.end());
            indices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = static_cast<u32>(sizeof(u32) * cubeMesh.indices.size()),
                .initialData = cubeMesh.indices.data()
            });
            indexCount = static_cast<u32>(cubeMesh.indices.size());

            auto right = LoadImage("Content/Assets/skybox/right.ktx2");
            auto left = LoadImage("Content/Assets/skybox/left.ktx2");
            auto top = LoadImage("Content/Assets/skybox/top.ktx2");
            auto bottom = LoadImage("Content/Assets/skybox/bottom.ktx2");
            auto front = LoadImage("Content/Assets/skybox/front.ktx2");
            auto back = LoadImage("Content/Assets/skybox/back.ktx2");

            std::vector<RHISubresourceData> subs;
            subs.reserve(6 * back.subs.size());
            subs.append_range(right.subs);
            subs.append_range(left.subs);
            subs.append_range(top.subs);
            subs.append_range(bottom.subs);
            subs.append_range(front.subs);
            subs.append_range(back.subs);

            cubeMap = device.CreateTexture(RHITextureCreateDesc{
                .width = back.width, .height = back.height,
                .mipLevels = back.mipLevels,
                .arraySize = 6,
                .format = back.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = subs
            });

            aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();
        }

        void ProcessInput(const InputProvider& input) override{
            if(!input.IsKeyDown(MouseButton::RButton))
                return;

            // radian per pixel
            constexpr f32 SENSITIVITY = 0.003f;
            // do not let the camera flip over the poles
            constexpr f32 PITCH_LIMIT = 0.5f * std::numbers::pi_v<f32> - 0.01f;

            const auto dpos = input.GetMouseDPos();

            yaw += dpos.x * SENSITIVITY;
            pitch = std::clamp(
                pitch + dpos.y * SENSITIVITY,
                -PITCH_LIMIT, PITCH_LIMIT
            );
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            const auto view = viewMat(zeros(), GetCameraRot());
            const auto proj = perspective(fovY, aspect, nearZ, farZ);

            Uniforms uniforms{
                .viewProj = proj * view
            };
            const auto frameCB = Device().UploadTransient(uniforms);

            std::array colorAttachments = {
                RHIColorAttachment{
                    .texture = backBuffer.texture,
                    .loadAction = backBuffer.loadAction,
                    .storeAction = backBuffer.storeAction,
                    .clearColor = Colors::Grey
                }
            };
            const std::array acquires{AcquireBackBuffer(backBuffer)};
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            }, acquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            cmdList.SetVertexBuffer(
                *vertices,
                0,
                sizeof(Vertex)
            );
            cmdList.SetPushGraphicsConstants(PushConstants{
                .texture = cubeMap->GetReadableID(RHITextureViewDesc{
                    .format = cubeMap->GetFormat(),
                    .config = RHITextureViewDesc::TexCube{}
                })
            });
            cmdList.SetGraphicsConstantBuffer(frameCB, 0);
            cmdList.DrawIndexed(
                RHIIndexBufferView{
                    .buffer = indices.get()
                },
                indexCount
            );

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }

        void OnResize(u32 width, u32 height) override{
            aspect = static_cast<f32>(width) / height;
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "CubeMapping",
        .width = 800, .height = 800,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<CubeMapping>(windowConfig);
}
