#include "ImageLoader.hpp"
#include "Primitives.hpp"
#include "AppFramework.hpp"

namespace Crowy
{
    class HelloTexture: public App{
        using App::App;

        RHIGraphicsPipelineStateRAII pso;
        RHITextureRAII texture;
        struct PushConstants{
            u64 textureID;
        };

        void OnInit(RHIDevice& device) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/Shader/HelloTexture.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/HelloTexture.slang",
                    .entryPoint = "fs_main"
                },
                .blend = RHIBlendState{
                    .renderTargets = {
                        RHIRenderTargetBlendState{
                            .blendEnable = true,
                            .srcBlend = RHIBlend::SrcAlpha,
                            .dstBlend = RHIBlend::InvSrcAlpha
                        }
                    }
                },
                .renderTargetFormats = {
                    swapchain->GetFormat()
                },
                .renderTargetCount = 1
            });

            auto image = LoadImage("Content/Assets/3crates/crate1/crate1_diffuse.ktx2");

            texture = device.CreateTexture(RHITextureCreateDesc{
                .width = image.width, .height = image.height,
                .mipLevels = image.mipLevels,
                .arraySize = image.arraySize,
                .format = image.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = image.subs
            });
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            std::array colorAttachments = {
                RHIColorAttachment{
                    .texture = backBuffer.texture,
                    .loadAction = backBuffer.loadAction,
                    .storeAction = backBuffer.storeAction,
                    .clearColor = Colors::Grey
                }
            };
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            });
            cmdList.SetViewport(FullViewport(*backBuffer.texture));
            cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

            cmdList.SetPipelineState(*pso);
            PushConstants pushConstants{
                .textureID = texture->GetReadableID(),
            };
            cmdList.SetPushGraphicsConstants(pushConstants);
            cmdList.Draw(4);

            cmdList.EndRenderPass();
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "HelloTexture",
        .width = 800, .height = 800,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<HelloTexture>(windowConfig);
}
