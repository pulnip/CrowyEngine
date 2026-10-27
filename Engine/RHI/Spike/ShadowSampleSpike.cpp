#include <array>

#include "AppFramework.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class ShadowSampleSpike: public App {
        using App::App;

        static constexpr CStr SHADER_PATH =
            "Engine/RHI/Spike/ShadowSampleSpike.slang";
        // not square, so a width and height swapped anywhere shows
        static constexpr u32 MAP_WIDTH = 256;
        static constexpr u32 MAP_HEIGHT = 128;
        static constexpr u32 PANE_SIZE = 512;

        struct SpikePush {
            u64 asTexture;
            u64 asDepthTexture;
            u32 parity;
            u32 _pad0 = 0;
        };

        RHITextureRAII depth;
        RHIGraphicsPipelineStateRAII writeDepth;
        RHIGraphicsPipelineStateRAII readTexture;
        RHIGraphicsPipelineStateRAII readDepthTexture;

        static RHIGraphicsPipelineStateDesc readDesc(
            RHIPixelFormat format,
            CStr fragmentEntry
        ) {
            return RHIGraphicsPipelineStateDesc{
                .vertexShader =
                    {.path = SHADER_PATH, .entryPoint = "vs_fullscreen"},
                .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader =
                    RHIShaderDesc{
                        .path = SHADER_PATH,
                        .entryPoint = fragmentEntry
                    },
                .renderTargetFormats = {format},
                .renderTargetCount = 1
            };
        }

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override {
            depth = device.CreateTexture(
                RHITextureCreateDesc{
                    .width = MAP_WIDTH,
                    .height = MAP_HEIGHT,
                    .format = RHIPixelFormat::D32_FLOAT,
                    .usage = combine(
                        RHITextureUsage::DepthStencil,
                        RHITextureUsage::ShaderResource
                    ),
                    .clearDepthStencil = {.depth = 1.0f}
                },
                "shadowSample"
            );

            writeDepth = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .vertexShader =
                        {.path = SHADER_PATH, .entryPoint = "vs_shapes"},
                    .rasterizer =
                        RHIRasterizerState{.cullMode = RHICullMode::None},
                    .depthStencil =
                        RHIDepthStencilState{
                            .format = RHIPixelFormat::D32_FLOAT,
                            .depthWriteEnable = true,
                            .depthFunc = RHIComparisonFunc::Less
                        },
                    .renderTargetCount = 0
                },
                "writeDepth"
            );

            readTexture = device.CreatePipelineState(
                readDesc(swapchain.GetFormat(), "fs_read_texture"),
                "readTexture"
            );
            readDepthTexture = device.CreatePipelineState(
                readDesc(swapchain.GetFormat(), "fs_read_depth_texture"),
                "readDepthTexture"
            );
        }

        void OnRecord(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override {
            const auto depthEdge = makeBarrier(
                *depth,
                RHIResourceUsage::DepthWrite,
                RHIResourceUsage::SampledFragment
            );
            const SpikePush push{
                .asTexture = depth->GetReadableID(),
                .asDepthTexture = depth->GetReadableID(),
                .parity = static_cast<u32>(FrameNumber() & 1)
            };

            {
                const std::array acquires{
                    // last frame's read; the clear drops what it read
                    makeCrossSubmissionBarrier(
                        *depth,
                        RHIResourceUsage::SampledFragment,
                        RHIResourceUsage::DepthWrite,
                        /*discardContents=*/true
                    )
                };
                cmdList.BeginRenderPass(
                    RHIRenderPassDesc{
                        .depthAttachment =
                            RHIDepthAttachment{
                                .texture = depth.get(),
                                .loadAction = RHILoadAction::Clear,
                                .storeAction = RHIStoreAction::Store,
                                .clearDepthStencil = {.depth = 1.0f}
                            }
                    },
                    acquires
                );
                cmdList.SetViewport(fullViewport(*depth));
                cmdList.SetScissorRect(fullScissorRect(*depth));

                cmdList.SetPipelineState(*writeDepth);
                cmdList.SetPushGraphicsConstants(push);
                cmdList.Draw(9);

                const std::array releases{depthEdge};
                cmdList.EndRenderPass(releases);
            }

            {
                std::array colorAttachments = {backBuffer};
                const std::array acquires{
                    depthEdge,
                    AcquireBackBuffer(backBuffer)
                };
                cmdList.BeginRenderPass(
                    RHIRenderPassDesc{.colorAttachments = colorAttachments},
                    acquires
                );

                drawPane(cmdList, *readTexture, 0, push);
                drawPane(cmdList, *readDepthTexture, PANE_SIZE, push);

                const std::array releases{ReleaseBackBuffer(backBuffer)};
                cmdList.EndRenderPass(releases);
            }
        }

        // D3D12 carries viewport and scissor across passes, so each pane sets
        // both
        void drawPane(
            RHICommandList& cmdList,
            RHIGraphicsPipelineState& pso,
            u32 left,
            const SpikePush& push
        ) {
            cmdList.SetViewport(
                RHIViewport{
                    .x = static_cast<f32>(left),
                    .y = 0,
                    .width = PANE_SIZE,
                    .height = PANE_SIZE,
                    .minDepth = 0,
                    .maxDepth = 1
                }
            );
            cmdList.SetScissorRect(
                RHIScissorRect{
                    .left = static_cast<i32>(left),
                    .top = 0,
                    .right = static_cast<i32>(left + PANE_SIZE),
                    .bottom = PANE_SIZE
                }
            );

            cmdList.SetPipelineState(pso);
            cmdList.SetPushGraphicsConstants(push);
            cmdList.Draw(3);
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "ShadowSampleSpike",
        .width = 1024,
        .height = 512,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<ShadowSampleSpike>(argc, argv, windowConfig);
}
