#include "AppFramework.hpp"
#include "EnumUtil.hpp"
#include "IntMath.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    namespace{
        u32 MipLevelCount(u32 width, u32 height){
            u32 count = 1;
            while((width > 1) || (height > 1)){
                width = halve(width);
                height = halve(height);
                ++count;
            }
            return count;
        }

        RHISubresourceRange MipRange(u32 mipLevel){
            return RHISubresourceRange{
                .firstMip = mipLevel,
                .numMips = 1,
                .firstArraySlice = 0,
                .numArraySlice = 1
            };
        }
    }

    // Multi-scale bloom over a mip chain.
    // The kernel keeps a constant one-texel spacing at every level;
    // the widening radius comes from each level living
    // one mip further down:
    //     accum = up(L0) + 1/2 up(L1) + 1/4 up(L2) + ...
    // Each level blurs the mip above it, so the chain is progressive and no pass
    // ever reads the mip it is writing.
    class BloomPass{
        // separable blur, overwrites its target
        RHIGraphicsPipelineStateRAII blur = nullptr;
        // point resample, adds into accum
        RHIGraphicsPipelineStateRAII upsampleAccum = nullptr;

        // mip i of both chains holds level i, at source size >> (i + 1)
        RHITextureRAII temp = nullptr;    // horizontal blur result
        RHITextureRAII blurred = nullptr; // vertical blur result, next level input
        // full resolution, single mip
        RHITextureRAII accum = nullptr;
        u32 levelCount = 0;

        struct BloomParam{
            u64 texture;
            Vec2 texelDir;
            f32 intensity;
        };

    public:
        void Init(RHIDevice& device, RHITexture& source, u32 levels = 4){
            RHIGraphicsPipelineStateDesc desc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/Shader/Bloom.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/Bloom.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    source.GetFormat()
                },
                .renderTargetCount = 1
            };
            blur = device.CreatePipelineState(desc, "bloomBlur");

            desc.blend = RHIBlendState{
                .renderTargets = {
                    RHIRenderTargetBlendState{
                        .blendEnable = true,
                        .srcBlend = RHIBlend::One,
                        .dstBlend = RHIBlend::One,
                        .blendOp = RHIBlendOp::Add,
                        // only the color accumulates, alpha stays at the clear value
                        .writeMask = RHIColorWriteMask::EnableColor
                    }
                }
            };
            upsampleAccum = device.CreatePipelineState(desc, "bloomUpsample");

            const auto usage = combine(
                RHITextureUsage::ShaderRead,
                RHITextureUsage::RenderTarget
            );

            accum = device.CreateTexture(RHITextureCreateDesc{
                .width = source.GetWidth(),
                .height = source.GetHeight(),
                .format = source.GetFormat(),
                .usage = usage
            }, "bloomAccum");

            // level 0 already runs at half the source,
            // so the chain starts there rather than at source size.
            const auto width = halve(source.GetWidth());
            const auto height = halve(source.GetHeight());
            levelCount = std::min(levels, MipLevelCount(width, height));

            const RHITextureCreateDesc chainDesc{
                .width = width,
                .height = height,
                .mipLevels = levelCount,
                .format = source.GetFormat(),
                .usage = usage
            };
            temp = device.CreateTexture(chainDesc, "bloomTemp");
            blurred = device.CreateTexture(chainDesc, "bloomBlurred");
        }

        RHITexture& Execute(RHITexture& source, RHICommandList& cmdList){
            for(u32 i=0; i<levelCount; ++i){
                // level 0 reads the source, deeper levels the mip above them
                RHITexture& input = i == 0 ? source : *blurred;
                const u32 inputMip = i == 0 ? 0 : i - 1;
                const auto inputRange = i == 0 ?
                    RHISubresourceRange{} :
                    MipRange(inputMip);

                {
                    std::array barriers = {
                        MakeBarrier(input, RHIResourceUsage::FragmentRead, inputRange),
                        MakeBarrier(*temp, RHIResourceUsage::RenderTarget, MipRange(i))
                    };
                    cmdList.TransitionBarrier(barriers);
                }

                // horizontal blur, halving on the way out. the kernel steps in
                // *input* texels, so 2 of them are 1 texel of the target mip.
                singleBlur(
                    cmdList,
                    *blur,
                    input, inputMip,
                    *temp, i,
                    Vec2{2, 0}, 1.0f,
                    RHILoadAction::DontCare
                );

                {
                    std::array barriers = {
                        MakeBarrier(*temp, RHIResourceUsage::FragmentRead, MipRange(i)),
                        MakeBarrier(*blurred, RHIResourceUsage::RenderTarget, MipRange(i))
                    };
                    cmdList.TransitionBarrier(barriers);
                }

                // vertical blur, already at the target resolution
                singleBlur(
                    cmdList,
                    *blur,
                    *temp, i,
                    *blurred, i,
                    Vec2{0, 1}, 1.0f,
                    RHILoadAction::DontCare
                );

                {
                    std::array barriers = {
                        MakeBarrier(*blurred, RHIResourceUsage::FragmentRead, MipRange(i)),
                        MakeBarrier(*accum, RHIResourceUsage::RenderTarget)
                    };
                    cmdList.TransitionBarrier(barriers);
                }

                // bilinear upsample back to full resolution, added on top of the coarser levels.
                // a zero step collapses the kernel to a resample.
                singleBlur(
                    cmdList,
                    *upsampleAccum,
                    *blurred, i,
                    *accum, 0,
                    Vec2{0, 0}, 1.0f / static_cast<f32>(1u << i),
                    // accum must start empty every frame
                    i == 0 ? RHILoadAction::Clear : RHILoadAction::Load
                );
            }

            return *accum;
        }

    private:
        void singleBlur(
            RHICommandList& cmdList,
            RHIGraphicsPipelineState& pso,
            RHITexture& src, u32 srcMip,
            RHITexture& dst, u32 dstMip,
            Vec2 texelDir,
            f32 intensity,
            RHILoadAction loadAction
        ){
            std::array colorAttachments = {
                RHIColorAttachment{
                    .texture = &dst,
                    .loadAction = loadAction,
                    .storeAction = RHIStoreAction::Store,
                    .mipLevel = dstMip
                }
            };
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
            });
            cmdList.SetViewport(FullViewport(dst, dstMip));
            cmdList.SetScissorRect(FullScissorRect(dst, dstMip));

            cmdList.SetPipelineState(pso);
            cmdList.SetPushGraphicsConstants(BloomParam{
                // a single-mip view, so Sample and GetDimensions in the shader
                // both see that mip and nothing else
                .texture = src.GetReadableID(RHITextureViewDesc{
                    .format = src.GetFormat(),
                    .mostDetailedMip = srcMip,
                    .mipCount = 1
                }),
                .texelDir = texelDir,
                .intensity = intensity
            });
            cmdList.Draw(4);

            cmdList.EndRenderPass();
        }
    };

    class BlackholeSimulation: public App{
        using App::App;

        RHIGraphicsPipelineStateRAII diskGenerator = nullptr;
        RHITextureRAII disk = nullptr;
        struct GenerationParam{
            f32 rs;
            f32 diskInner;
            f32 diskOuter;
            f32 maxTempKelvin;
        };

        RHIGraphicsPipelineStateRAII blackholeSimulator = nullptr;
        RHITextureRAII scene = nullptr;
        struct PushConstants{
            u64 disk;
        };
        static constexpr u32 PLANET_COUNT = 3;
        struct SimulationParam{
            Vec3 bhPos{0.0, 0.0, 3.0f};
            f32 rs = 1.0f;
            Vec3 camPos{30.0f, 5.0f, 0.0f};
            f32 aspect;
            Vec3 camRight = unitX();
            f32 tanHalfFov = std::tanf(0.5f * static_cast<f32>(toRadian(45.0f)));
            Vec3 camUp = unitY();
            f32 diskInner = 3.0f;
            Vec3 camForward = unitZ();
            f32 diskOuter = 10.0f;
            Vec4 posRadius[PLANET_COUNT] = {
                Vec4{  0, 0,  15,   2},
                Vec4{ 20, 0, -10, 2.5},
                Vec4{-15, 0, -15,   3}
            };
            Color planetColor[PLANET_COUNT] = {
                Color{0.5f, 0.1f, 0.1f, 1},
                Color{0.0f, 0.5f, 1.0f, 1},
                Color{0.4f, 0.7f, 0.1f, 1}
            };
            f32 elapsedTimeSeconds = 0.0f;
        } simParam;
        RHIBufferRAII simParamBuffer = nullptr;

        BloomPass bloomPass;
        RHITextureRAII brightMask = nullptr;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            diskGenerator = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/Shader/BlackholeDisk.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/BlackholeDisk.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });
            disk = device.CreateTexture(RHITextureCreateDesc{
                .width = swapchain.GetWidth(),
                .height = swapchain.GetHeight(),
                .format = RHIPixelFormat::RGBA8_UNORM,
                .usage = combine(
                    RHITextureUsage::RenderTarget,
                    RHITextureUsage::ShaderRead
                )
            });

            blackholeSimulator = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/Shader/Blackhole.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/Blackhole.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat(),
                    swapchain.GetFormat()
                },
                .renderTargetCount = 2
            });
            scene = device.CreateTexture(RHITextureCreateDesc{
                .width = swapchain.GetWidth(), .height = swapchain.GetHeight(),
                .format = swapchain.GetFormat(),
                .usage = combine(
                    RHITextureUsage::ShaderRead,
                    RHITextureUsage::RenderTarget
                )
            }, "scene");

            simParam.camForward = normalize(simParam.bhPos - simParam.camPos);
            simParam.camRight   = normalize(cross(unitY(), simParam.camForward));
            simParam.camUp      = cross(simParam.camForward, simParam.camRight);

            simParam.aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();

            simParamBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(simParam),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite,
                .initialData = &simParam
            });

            brightMask = device.CreateTexture(RHITextureCreateDesc{
                .width = swapchain.GetWidth(), .height = swapchain.GetHeight(),
                .format = swapchain.GetFormat(),
                .usage = combine(
                    RHITextureUsage::ShaderRead,
                    RHITextureUsage::RenderTarget
                )
            }, "brightMask");
            bloomPass.Init(device, *brightMask, 4);
        }

        void OnInitialRecord(RHICommandList& cmdList) override{
            cmdList.TransitionBarrier(
                *disk,
                RHIResourceUsage::RenderTarget
            );

            {
                std::array colorAttachments = {
                    RHIColorAttachment{
                        .texture = disk.get(),
                        .loadAction = RHILoadAction::DontCare,
                        .storeAction = RHIStoreAction::Store
                    }
                };
                cmdList.BeginRenderPass(RHIRenderPassDesc{
                    .colorAttachments = colorAttachments,
                });
                cmdList.SetViewport(FullViewport(*disk));
                cmdList.SetScissorRect(FullScissorRect(*disk));

                cmdList.SetPipelineState(*diskGenerator);
                cmdList.SetPushGraphicsConstants(GenerationParam{
                    .rs = simParam.rs,
                    .diskInner = simParam.diskInner,
                    .diskOuter = simParam.diskOuter,
                    .maxTempKelvin = 1e+4
                });
                cmdList.Draw(4);

                cmdList.EndRenderPass();
            }

            cmdList.TransitionBarrier(
                *disk,
                RHIResourceUsage::FragmentRead
            );
        }

        void OnUpdate(f64, f64 elapsedTime) override{
            simParam.elapsedTimeSeconds = static_cast<f32>(elapsedTime);
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            simParamBuffer->Upload(simParam);

            {
                std::array barriers = {
                    MakeBarrier(*scene, RHIResourceUsage::RenderTarget),
                    MakeBarrier(*brightMask, RHIResourceUsage::RenderTarget)
                };
                cmdList.TransitionBarrier(barriers);
            }

            {
                std::array colorAttachments = {
                    RHIColorAttachment{
                        .texture = scene.get(),
                        .loadAction = RHILoadAction::DontCare,
                        .storeAction = RHIStoreAction::Store
                    },
                    RHIColorAttachment{
                        .texture = brightMask.get(),
                        .loadAction = RHILoadAction::Clear,
                        .storeAction = RHIStoreAction::Store
                    }
                };
                cmdList.BeginRenderPass(RHIRenderPassDesc{
                    .colorAttachments = colorAttachments
                });
                cmdList.SetViewport(FullViewport(*backBuffer.texture));
                cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

                cmdList.SetPipelineState(*blackholeSimulator);
                cmdList.SetPushGraphicsConstants(PushConstants{
                    .disk = disk->GetReadableID()
                });
                cmdList.SetGraphicsConstantBuffer(*simParamBuffer, 0);
                cmdList.Draw(4);

                cmdList.EndRenderPass();
            }

            RHITexture& bloom = bloomPass.Execute(*brightMask, cmdList);

            {
                std::array barriers = {
                    MakeBarrier(bloom, RHIResourceUsage::CopySrc),
                    MakeBarrier(*backBuffer.texture, RHIResourceUsage::CopyDst)
                };
                cmdList.TransitionBarrier(barriers);
            }

            {
                cmdList.BeginBlit();

                cmdList.Copy(bloom, *backBuffer.texture);

                cmdList.EndBlit();
            }
        }

        void OnResize(u32 width, u32 height) override{
            simParam.aspect = static_cast<f32>(width) / height;
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "BlackholeSimulation",
        .width = 1280, .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<BlackholeSimulation>(windowConfig);
}
