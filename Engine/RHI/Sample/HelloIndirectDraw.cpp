#include <array>
#include "AppFramework.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    // Indirect twin of BaseInstanceSpike:
    // the same four quads, but the draw arguments live in a GPU buffer
    // and reach the hardware through a single ExecuteIndirectIndexed call.
    // Rendering must be pixel-identical to BaseInstanceSpike
    class HelloIndirectDraw: public App{
        using App::App;

        static constexpr u32 DRAW_COUNT = 4;

        RHIGraphicsPipelineStateRAII pso;
        RHIBufferRAII quadIndices;
        RHIBufferRAII drawArgs;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/RHI/Spike/BaseInstanceSpike.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .fragmentShader = {
                    .path = "Engine/RHI/Spike/BaseInstanceSpike.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1,
                .profile = "sm_6_8"
            });

            constexpr std::array<u32, 6> indices = {
                0, 1, 2,
                0, 2, 3
            };
            quadIndices = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(indices),
                .initialData = indices.data()
            });

            std::array<RHIDrawIndexedArgs, DRAW_COUNT> args;
            for(u32 i=0; i<DRAW_COUNT; ++i){
                args[i] = RHIDrawIndexedArgs{
                    .indexCount = indices.size(),
                    .baseInstance = i + 1
                };
            }
            drawArgs = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(args),
                .memory = RHIMemoryType::CPUWrite,
                .initialData = args.data()
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

            cmdList.ExecuteIndirectIndexed(DrawBatchIndexed{
                .pso = pso.get(),
                .args = drawArgs.get(),
                .drawCount = DRAW_COUNT,
                .indices = RHIIndexBufferView{
                    .buffer = quadIndices.get()
                }
            });

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "HelloIndirectDraw",
        .width = 800, .height = 800,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<HelloIndirectDraw>(windowConfig);
}
