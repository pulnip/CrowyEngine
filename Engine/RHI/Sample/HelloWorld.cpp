#include "AppFramework.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class HelloWorld: public App {
        RHIGraphicsPipelineStateRAII pso;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override {
            pso = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader =
                        {.path = "Engine/RHI/Sample/Triangle.slang",
                         .entryPoint = "vs_main"},
                    .fragmentShader =
                        RHIShaderDesc{
                            .path = "Engine/RHI/Sample/Triangle.slang",
                            .entryPoint = "fs_main"
                        },
                    .renderTargetFormats = {swapchain.GetFormat()},
                    .renderTargetCount = 1
                }
            );
        }

        void OnRecord(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override {
            std::array colorAttachments = {backBuffer};
            const std::array acquires{AcquireBackBuffer(backBuffer)};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colorAttachments},
                acquires
            );
            cmdList.SetViewport(fullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(fullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
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
    return Main<HelloWorld>(argc, argv, windowConfig);
}
