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

        // the caller's producing pass must have released
        // `MakeBarrier(source, RenderTarget, SampledFragment)` (whole range);
        // the first horizontal blur closes that edge here. the result stays
        // released as `MakeBarrier(result, RenderTarget, SampledFragment)` -
        // acquire that same edge in the pass that samples it.
        RHITexture& Execute(RHITexture& source, RHICommandList& cmdList){
            using enum RHIResourceUsage;

            for(u32 i=0; i<levelCount; ++i){
                // level 0 reads the source, deeper levels the mip above them
                RHITexture& input = i == 0 ? source : *blurred;
                const u32 inputMip = i == 0 ? 0 : i - 1;
                const auto inputRange = i == 0 ?
                    RHISubresourceRange{} :
                    MipRange(inputMip);

                // i == 0: closes the caller's source edge (first consumer).
                // i > 0: second consumer of blurred mip i-1 - the upsample
                // already closed it, so the backend emits a sync-only barrier
                const auto inputAcquire = MakeBarrier(
                    input, RenderTarget, SampledFragment, inputRange
                );

                // horizontal blur, halving on the way out. the kernel steps in
                // *input* texels, so 2 of them are 1 texel of the target mip.
                {
                    const std::array acquires{
                        inputAcquire,
                        // last frame's vertical blur read this mip (WAR);
                        // it is fully overwritten, so drop the contents
                        MakeCrossSubmissionBarrier(
                            *temp, SampledFragment, RenderTarget,
                            /*discardContents=*/true, MipRange(i)
                        )
                    };
                    const std::array releases{
                        MakeBarrier(*temp, RenderTarget, SampledFragment, MipRange(i))
                    };
                    singleBlur(
                        cmdList,
                        *blur,
                        input, inputMip,
                        *temp, i,
                        Vec2{2, 0}, 1.0f,
                        RHILoadAction::DontCare,
                        acquires, releases
                    );
                }

                // vertical blur, already at the target resolution
                {
                    const std::array acquires{
                        MakeBarrier(*temp, RenderTarget, SampledFragment, MipRange(i)),
                        MakeCrossSubmissionBarrier(
                            *blurred, SampledFragment, RenderTarget,
                            /*discardContents=*/true, MipRange(i)
                        )
                    };
                    const std::array releases{
                        MakeBarrier(*blurred, RenderTarget, SampledFragment, MipRange(i))
                    };
                    singleBlur(
                        cmdList,
                        *blur,
                        *temp, i,
                        *blurred, i,
                        Vec2{0, 1}, 1.0f,
                        RHILoadAction::DontCare,
                        acquires, releases
                    );
                }

                // bilinear upsample back to full resolution, added on top of the coarser levels.
                // a zero step collapses the kernel to a resample.
                {
                    const std::array acquires{
                        MakeBarrier(*blurred, RenderTarget, SampledFragment, MipRange(i)),
                        // level 0 recycles accum from last frame's composite
                        // read; later levels chain WAW on their own blend work
                        i == 0 ?
                            MakeCrossSubmissionBarrier(
                                *accum, SampledFragment, RenderTarget,
                                /*discardContents=*/true
                            ) :
                            MakeBarrier(*accum, RenderTarget, RenderTarget)
                    };
                    const std::array releases{
                        // the last level hands accum to whoever samples the
                        // result; earlier levels feed the next blend
                        i + 1 == levelCount ?
                            MakeBarrier(*accum, RenderTarget, SampledFragment) :
                            MakeBarrier(*accum, RenderTarget, RenderTarget)
                    };
                    singleBlur(
                        cmdList,
                        *upsampleAccum,
                        *blurred, i,
                        *accum, 0,
                        Vec2{0, 0}, 1.0f / static_cast<f32>(1u << i),
                        // accum must start empty every frame
                        i == 0 ? RHILoadAction::Clear : RHILoadAction::Load,
                        acquires, releases
                    );
                }
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
            RHILoadAction loadAction,
            std::span<const RHITextureBarrier> acquires,
            std::span<const RHITextureBarrier> releases
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
            }, acquires);
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

            cmdList.EndRenderPass(releases);
        }
    };

    class BlackholeSimulation: public App{
        using App::App;

        // linear radiance. the g^3 doppler boost drives the disk well past 1,
        // which is exactly what an UNORM target used to throw away.
        static constexpr auto HDR_FORMAT = RHIPixelFormat::RGBA16_FLOAT;

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
        RHIBufferSlice simParamBuffer;

        BloomPass bloomPass;
        RHITextureRAII brightMask = nullptr;

        RHIGraphicsPipelineStateRAII composite = nullptr;
        struct CompositeParam{
            u64 scene;
            u64 bloom;
            // brightMask carries the whole disk color, not just the excess over the threshold,
            // so adding it back at full strength would double the disk
            f32 bloomStrength = 0.2f;
            f32 exposure = 1.0f;
        };

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
                    HDR_FORMAT,
                    HDR_FORMAT
                },
                .renderTargetCount = 2
            });
            scene = device.CreateTexture(RHITextureCreateDesc{
                .width = swapchain.GetWidth(), .height = swapchain.GetHeight(),
                .format = HDR_FORMAT,
                .usage = combine(
                    RHITextureUsage::ShaderRead,
                    RHITextureUsage::RenderTarget
                )
            }, "scene");

            simParam.camForward = normalize(simParam.bhPos - simParam.camPos);
            simParam.camRight   = normalize(cross(unitY(), simParam.camForward));
            simParam.camUp      = cross(simParam.camForward, simParam.camRight);

            simParam.aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();

            brightMask = device.CreateTexture(RHITextureCreateDesc{
                .width = swapchain.GetWidth(), .height = swapchain.GetHeight(),
                .format = HDR_FORMAT,
                .usage = combine(
                    RHITextureUsage::ShaderRead,
                    RHITextureUsage::RenderTarget
                )
            }, "brightMask");
            bloomPass.Init(device, *brightMask, 4);

            composite = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/Shader/Composite.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = {
                    .path = "Engine/Shader/Composite.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            }, "composite");
        }

        void OnInitialRecord(RHICommandList& cmdList) override{
            std::array colorAttachments = {
                RHIColorAttachment{
                    .texture = disk.get(),
                    .loadAction = RHILoadAction::DontCare,
                    .storeAction = RHIStoreAction::Store
                }
            };
            const std::array acquires{
                // first use of the freshly created texture
                MakeBarrier(*disk,
                    RHIResourceUsage::Undefined,
                    RHIResourceUsage::RenderTarget
                )
            };
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
            }, acquires);
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

            // every consumer lives in later frames' command lists, so this
            // release completes at Close as the hand-off to sampled use
            const std::array releases{
                MakeBarrier(*disk,
                    RHIResourceUsage::RenderTarget,
                    RHIResourceUsage::SampledFragment
                )
            };
            cmdList.EndRenderPass(releases);
        }

        void OnUpdate(f64, f64 elapsedTime) override{
            simParam.elapsedTimeSeconds = static_cast<f32>(elapsedTime);
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            simParamBuffer = Device().UploadTransient(simParam);

            // the scene edge skips the whole bloom chain: released by the
            // simulation pass, acquired only by the composite - the
            // non-adjacent dependency this barrier model exists for
            const auto sceneEdge = MakeBarrier(*scene,
                RHIResourceUsage::RenderTarget,
                RHIResourceUsage::SampledFragment
            );
            // brightMask goes to the bloom's first blur instead (adjacent)
            const auto brightMaskEdge = MakeBarrier(*brightMask,
                RHIResourceUsage::RenderTarget,
                RHIResourceUsage::SampledFragment
            );

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
                const std::array acquires{
                    // both were sampled by last frame's passes (WAR) and are
                    // fully rewritten, so the contents can go
                    MakeCrossSubmissionBarrier(*scene,
                        RHIResourceUsage::SampledFragment,
                        RHIResourceUsage::RenderTarget,
                        /*discardContents=*/true
                    ),
                    MakeCrossSubmissionBarrier(*brightMask,
                        RHIResourceUsage::SampledFragment,
                        RHIResourceUsage::RenderTarget,
                        /*discardContents=*/true
                    )
                };
                cmdList.BeginRenderPass(RHIRenderPassDesc{
                    .colorAttachments = colorAttachments
                }, acquires);
                cmdList.SetViewport(FullViewport(*backBuffer.texture));
                cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

                cmdList.SetPipelineState(*blackholeSimulator);
                cmdList.SetPushGraphicsConstants(PushConstants{
                    // generated once in OnInitialRecord and handed off there
                    .disk = disk->GetReadableID()
                });
                cmdList.SetGraphicsConstantBuffer(simParamBuffer, 0);
                cmdList.Draw(4);

                const std::array releases{sceneEdge, brightMaskEdge};
                cmdList.EndRenderPass(releases);
            }

            RHITexture& bloom = bloomPass.Execute(*brightMask, cmdList);

            {
                std::array colorAttachments = {
                    RHIColorAttachment{
                        .texture = backBuffer.texture,
                        // a full screen quad covers every pixel
                        .loadAction = RHILoadAction::DontCare,
                        .storeAction = backBuffer.storeAction
                    }
                };
                const std::array acquires{
                    sceneEdge,
                    // the edge Execute's last upsample released
                    MakeBarrier(bloom,
                        RHIResourceUsage::RenderTarget,
                        RHIResourceUsage::SampledFragment
                    ),
                    AcquireBackBuffer(backBuffer)
                };
                cmdList.BeginRenderPass(RHIRenderPassDesc{
                    .colorAttachments = colorAttachments
                }, acquires);
                cmdList.SetViewport(FullViewport(*backBuffer.texture));
                cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

                cmdList.SetPipelineState(*composite);
                cmdList.SetPushGraphicsConstants(CompositeParam{
                    .scene = scene->GetReadableID(),
                    .bloom = bloom.GetReadableID()
                });
                cmdList.Draw(4);

                const std::array releases{ReleaseBackBuffer(backBuffer)};
                cmdList.EndRenderPass(releases);
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
