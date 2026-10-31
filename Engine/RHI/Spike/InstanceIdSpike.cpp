#include <array>

#include "AppFramework.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class InstanceIdSpike: public App {
        static constexpr u32 ROW_COUNT = 3;
        static constexpr u32 INSTANCES_PER_ROW = 4;

        RHIGraphicsPipelineStateRAII pso;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override {
            pso = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader =
                        {.path = "Engine/RHI/Spike/InstanceIdSpike.slang",
                         .entryPoint = "vs_main"},
                    .rasterizer =
                        RHIRasterizerState{.cullMode = RHICullMode::None},
                    .fragmentShader =
                        RHIShaderDesc{
                            .path = "Engine/RHI/Spike/InstanceIdSpike.slang",
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
            for(u32 row = 0; row < ROW_COUNT; ++row) {
                cmdList.Draw(4, INSTANCES_PER_ROW, 0, row + 1);
            }

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .width = 800,
        .height = 600,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .resizable = true,
    };
    return Main<InstanceIdSpike>(argc, argv, windowConfig);
}
