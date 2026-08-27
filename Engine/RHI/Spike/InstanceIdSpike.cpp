#include <array>
#include "AppFramework.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    // Companion to BaseInstanceSpike: that one proved baseInstance reaches the
    // shader as SV_StartInstanceLocation, this one guards that the zero-based
    // instance index (zeroBasedInstanceID in InstanceIndex.slang) really is
    // zero-based on every backend. Slang's Metal target emits [[instance_id]]
    // raw, which counts from the base instance - the helper subtracts it there.
    //
    // Three draws of four instances each, baseInstance 1, 2, 3. Row comes from
    // the draw, column from the zero-based index.
    //
    //   PASS: three left-aligned rows of four quads, columns 0-3 in every row.
    //   FAIL: a staircase - row 2 starting at column 2, row 3 at column 3 -
    //         which means a base instance leaked through the helper.
    //
    // OrbitFrame addresses trail segments by SV_InstanceID while carrying
    // the body index in baseInstance, so the zero-based reading is load-bearing
    // there; a segment is sub-pixel in that sample and could never show this.
    class InstanceIdSpike: public App{
        using App::App;

        static constexpr u32 ROW_COUNT = 3;
        static constexpr u32 INSTANCES_PER_ROW = 4;

        RHIGraphicsPipelineStateRAII pso;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/RHI/Spike/InstanceIdSpike.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .fragmentShader = {
                    .path = "Engine/RHI/Spike/InstanceIdSpike.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1,
                .profile = "sm_6_8"
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
            for(u32 row=0; row<ROW_COUNT; ++row){
                cmdList.Draw(4, INSTANCES_PER_ROW, 0, row + 1);
            }

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "InstanceIdSpike",
        .width = 800, .height = 600,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<InstanceIdSpike>(windowConfig);
}
