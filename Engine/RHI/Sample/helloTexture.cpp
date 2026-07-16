#include "ImageLoader.hpp"
#include "Primitives.hpp"
#include "AppFramework.hpp"

namespace Crowy
{
    class HelloTexture: public App{
        using App::App;

        RHIGraphicsPipelineStateRAII pso;
        RHITextureRAII diffuse;
        RHITextureRAII normal;
        RHITextureRAII bump;

        struct PushConstants{
            u64 diffuse;
            u64 normal;
            u64 bump;
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

            auto diffuse = LoadImage("Content/Assets/3crates/crate1/crate1_diffuse.ktx2");
            auto normal = LoadImage("Content/Assets/3crates/crate1/crate1_normal.ktx2");
            auto bump = LoadImage("Content/Assets/3crates/crate1/crate1_bump.ktx2");

            this->diffuse = device.CreateTexture(RHITextureCreateDesc{
                .width = diffuse.width, .height = diffuse.height,
                .mipLevels = diffuse.mipLevels,
                .arraySize = diffuse.arraySize,
                .format = diffuse.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = diffuse.subs
            });
            this->normal = device.CreateTexture(RHITextureCreateDesc{
                .width = normal.width, .height = normal.height,
                .mipLevels = normal.mipLevels,
                .arraySize = normal.arraySize,
                .format = normal.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = normal.subs
            });
            this->bump = device.CreateTexture(RHITextureCreateDesc{
                .width = bump.width, .height = bump.height,
                .mipLevels = bump.mipLevels,
                .arraySize = bump.arraySize,
                .format = bump.format,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = bump.subs
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
                .diffuse = diffuse->GetReadableID(),
                .normal = normal->GetReadableID(),
                .bump = bump->GetReadableID(),
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
