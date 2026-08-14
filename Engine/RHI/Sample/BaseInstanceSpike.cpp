#include "AppFramework.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    // Verifies that SV_StartInstanceLocation (SM6.8) delivers the
    // baseInstance of each draw: four DrawIndexed calls carry
    // baseInstance 1..4, and the vertex shader derives both column
    // and color from it (red, green, blue, white on black).
    class BaseInstanceSpike: public App{
        using App::App;

        static constexpr u32 DRAW_COUNT = 4;

        RHIGraphicsPipelineStateRAII pso;
        RHIBufferRAII quadIndices;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/BaseInstanceSpike.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .fragmentShader = {
                    .path = "Engine/Shader/BaseInstanceSpike.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1,
                .profile = "sm_6_8"
            });

            static constexpr u32 indices[] = {0, 1, 2, 0, 2, 3};
            quadIndices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(indices),
                .usage = RHIBufferUsage::IndexBuffer,
                .access = RHIMemoryAccess::GPUOnly,
                .initialData = indices
            });
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            std::array colorAttachments = {backBuffer};
            const std::array acquires{AcquireBackBuffer(backBuffer)};
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            }, acquires);
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);

            // drawIDs start at 1 so a semantic that silently reads 0
            // cannot pass as the first draw
            for(u32 drawID = 1; drawID <= DRAW_COUNT; ++drawID){
                cmdList.DrawIndexed(
                    RHIIndexBufferView{
                        .buffer = quadIndices.get()
                    },
                    6,
                    1,
                    0,
                    0,
                    drawID
                );
            }

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "BaseInstanceSpike",
        .width = 800, .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<BaseInstanceSpike>(windowConfig);
}
