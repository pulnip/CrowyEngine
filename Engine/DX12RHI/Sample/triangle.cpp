#include "Sample.hpp"

namespace Crowy
{
    class TriangleSample: public Sample{
        using Sample::Sample;

        RAII<DX12GraphicsPipelineState> pso;

        void OnInit(DX12Device& device) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/Triangle.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/Triangle.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    RHIPixelFormat::RGBA8_UNORM
                },
                .renderTargetCount = 1
            });
        }

        void OnRecord(DX12CommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            std::array colorAttachments = {backBuffer};
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            });
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            cmdList.Draw(3);

            cmdList.EndRenderPass();
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "DX12 Triangle Sample",
        .width = 800, .height = 800,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<TriangleSample>(windowConfig);
}
