#include "RHIBuffer.hpp"
#include "Sample.hpp"

namespace Crowy
{
    class HelloConstantBuffer: public Sample{
        using Sample::Sample;

        struct SceneData{
            Vec4 offset{0, 0, 0, 0};
        } sceneData;

        RAII<DX12GraphicsPipelineState> pso;
        RHIBufferRAII constantBuffer;

        void OnInit(DX12Device& device) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleList,
                    .vertexShader = {
                        .path = "Engine/Shader/HelloConstantBuffer.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/HelloConstantBuffer.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    RHIPixelFormat::RGBA8_UNORM
                },
                .renderTargetCount = 1
            });

            constantBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(sceneData),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite,
                .initialData = &sceneData
            });
        }

        void OnUpdate(f64 dt, f64 et) override{
            constexpr f32 scale = 0.5f;
            sceneData.offset.x = scale * std::cosf(et);
            sceneData.offset.y = scale * std::sinf(et);
        }

        void OnRecord(DX12CommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            constantBuffer->Upload(sceneData);

            std::array colorAttachments = {backBuffer};
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            });
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            cmdList.SetGraphicsConstantBuffer(
                *constantBuffer,
                0
            );
            cmdList.Draw(3);

            cmdList.EndRenderPass();
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "DX12 HelloConstantBuffer",
        .width = 800, .height = 800,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<HelloConstantBuffer>(windowConfig);
}
