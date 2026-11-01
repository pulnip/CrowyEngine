#include "Main.hpp"
#include "OS.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class HelloConstantBuffer: public MainLoop {
        struct SceneData {
            Vec4 offset{0, 0, 0, 0};
        } sceneData;

        RHIGraphicsPipelineStateRAII pso;

        void init(RHIDevice& device, RHISwapchain& swapchain) override {
            pso = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader =
                        {.path = "Engine/RHI/Sample/HelloConstantBuffer.slang",
                         .entryPoint = "vs_main"},
                    .fragmentShader =
                        RHIShaderDesc{
                            .path =
                                "Engine/RHI/Sample/HelloConstantBuffer.slang",
                            .entryPoint = "fs_main"
                        },
                    .renderTargetFormats = {swapchain.GetFormat()},
                    .renderTargetCount = 1
                }
            );
        }

        void update(f64 dt, f64 et) override {
            constexpr f32 scale = 0.5f;
            sceneData.offset.x = scale * std::cosf(et);
            sceneData.offset.y = scale * std::sinf(et);
        }

        void record(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override {
            const auto scene = OS::Get()->Device().UploadTransient(sceneData);

            std::array colorAttachments = {backBuffer};
            const std::array acquires{AcquireBackBuffer(backBuffer)};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colorAttachments},
                acquires
            );
            cmdList.SetViewport(fullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(fullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            cmdList.SetGraphicsConstantBuffer(scene, 0);
            cmdList.Draw(3);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
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
    return Main<HelloConstantBuffer>(argc, argv, windowConfig);
}
