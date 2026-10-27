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
    namespace
    {
        u32 MipLevelCount(u32 width, u32 height) {
            u32 count = 1;
            while((width > 1) || (height > 1)) {
                width = halve(width);
                height = halve(height);
                ++count;
            }
            return count;
        }

        RHISubresourceRange MipRange(u32 mipLevel) {
            return RHISubresourceRange{
                .firstMip = mipLevel,
                .numMips = 1,
                .firstArraySlice = 0,
                .numArraySlice = 1
            };
        }
    }

    // Each level blurs the mip above it with a one-texel kernel, so the radius
    // widens one mip per level and no pass reads the mip it is writing.
    class BloomPass {
        RHIGraphicsPipelineStateRAII blur = nullptr;
        RHIGraphicsPipelineStateRAII upsampleAccum = nullptr;

        RHITextureRAII temp = nullptr;
        RHITextureRAII blurred = nullptr;
        RHITextureRAII accum = nullptr;
        u32 levelCount = 0;

        struct BloomParam {
            u64 texture;
            Vec2 texelDir;
            f32 intensity;
        };

    public:
        void Init(RHIDevice& device, RHITexture& source) {
            RHIGraphicsPipelineStateDesc desc{
                .topology = RHIPrimitiveTopology::TriangleStrip,
                .vertexShader =
                    {.path = "Engine/RHI/Sample/Bloom.slang",
                     .entryPoint = "vs_main"},
                .fragmentShader =
                    RHIShaderDesc{
                        .path = "Engine/RHI/Sample/Bloom.slang",
                        .entryPoint = "fs_main"
                    },
                .renderTargetFormats = {source.GetFormat()},
                .renderTargetCount = 1
            };
            blur = device.CreatePipelineState(desc, "bloomBlur");

            desc.blend = RHIBlendState{
                .renderTargets = {RHIRenderTargetBlendState{
                    .blendEnable = true,
                    .srcBlend = RHIBlend::One,
                    .dstBlend = RHIBlend::One,
                    .blendOp = RHIBlendOp::Add,
                    .writeMask = RHIColorWriteMask::EnableColor
                }}
            };
            upsampleAccum = device.CreatePipelineState(desc, "bloomUpsample");
        }

        // sized after the source, so called again whenever it is recreated
        void CreateTargets(
            RHIDevice& device,
            RHITexture& source,
            u32 levels = 4
        ) {
            const auto usage = combine(
                RHITextureUsage::ShaderRead,
                RHITextureUsage::RenderTarget
            );

            accum = device.CreateTexture(
                RHITextureCreateDesc{
                    .width = source.GetWidth(),
                    .height = source.GetHeight(),
                    .format = source.GetFormat(),
                    .usage = usage
                },
                "bloomAccum"
            );

            // level 0 already runs at half the source
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

        // the caller releases source RenderTarget → SampledFragment (whole
        // range) and acquires that same edge on the result where it samples it
        RHITexture& Execute(RHITexture& source, RHICommandList& cmdList) {
            using enum RHIResourceUsage;

            for(u32 i = 0; i < levelCount; ++i) {
                RHITexture& input = i == 0 ? source : *blurred;
                const u32 inputMip = i == 0 ? 0 : i - 1;
                const auto inputRange =
                    i == 0 ? RHISubresourceRange{} : MipRange(inputMip);

                // i == 0 closes the caller's source edge; i > 0 is a second
                // consumer of blurred mip i-1, so the barrier is sync-only
                const auto inputAcquire = makeBarrier(
                    input,
                    RenderTarget,
                    SampledFragment,
                    inputRange
                );

                // the kernel steps in input texels, so a step of 2 is one texel
                // of the halved target mip
                {
                    const std::array acquires{
                        inputAcquire,
                        // last frame's vertical blur read this mip (WAR);
                        // it is fully overwritten, so drop the contents
                        makeCrossSubmissionBarrier(
                            *temp,
                            SampledFragment,
                            RenderTarget,
                            /*discardContents=*/true,
                            MipRange(i)
                        )
                    };
                    const std::array releases{makeBarrier(
                        *temp,
                        RenderTarget,
                        SampledFragment,
                        MipRange(i)
                    )};
                    singleBlur(
                        cmdList,
                        *blur,
                        input,
                        inputMip,
                        *temp,
                        i,
                        Vec2{2, 0},
                        1.0f,
                        RHILoadAction::DontCare,
                        acquires,
                        releases
                    );
                }

                {
                    const std::array acquires{
                        makeBarrier(
                            *temp,
                            RenderTarget,
                            SampledFragment,
                            MipRange(i)
                        ),
                        makeCrossSubmissionBarrier(
                            *blurred,
                            SampledFragment,
                            RenderTarget,
                            /*discardContents=*/true,
                            MipRange(i)
                        )
                    };
                    const std::array releases{makeBarrier(
                        *blurred,
                        RenderTarget,
                        SampledFragment,
                        MipRange(i)
                    )};
                    singleBlur(
                        cmdList,
                        *blur,
                        *temp,
                        i,
                        *blurred,
                        i,
                        Vec2{0, 1},
                        1.0f,
                        RHILoadAction::DontCare,
                        acquires,
                        releases
                    );
                }

                // a zero step collapses the kernel to a resample
                {
                    const std::array acquires{
                        makeBarrier(
                            *blurred,
                            RenderTarget,
                            SampledFragment,
                            MipRange(i)
                        ),
                        // level 0 recycles accum from last frame's composite
                        // read; later levels chain WAW on their own blend work
                        i == 0 ? makeCrossSubmissionBarrier(
                                     *accum,
                                     SampledFragment,
                                     RenderTarget,
                                     /*discardContents=*/true
                                 )
                               : makeBarrier(*accum, RenderTarget, RenderTarget)
                    };
                    const std::array releases{
                        // the last level hands accum to whoever samples the
                        // result; earlier levels feed the next blend
                        i + 1 == levelCount
                            ? makeBarrier(*accum, RenderTarget, SampledFragment)
                            : makeBarrier(*accum, RenderTarget, RenderTarget)
                    };
                    singleBlur(
                        cmdList,
                        *upsampleAccum,
                        *blurred,
                        i,
                        *accum,
                        0,
                        Vec2{0, 0},
                        1.0f / static_cast<f32>(1u << i),
                        i == 0 ? RHILoadAction::Clear : RHILoadAction::Load,
                        acquires,
                        releases
                    );
                }
            }

            return *accum;
        }

    private:
        void singleBlur(
            RHICommandList& cmdList,
            RHIGraphicsPipelineState& pso,
            RHITexture& src,
            u32 srcMip,
            RHITexture& dst,
            u32 dstMip,
            Vec2 texelDir,
            f32 intensity,
            RHILoadAction loadAction,
            std::span<const RHITextureBarrier> acquires,
            std::span<const RHITextureBarrier> releases
        ) {
            std::array colorAttachments = {RHIColorAttachment{
                .texture = &dst,
                .loadAction = loadAction,
                .storeAction = RHIStoreAction::Store,
                .mipLevel = dstMip
            }};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{
                    .colorAttachments = colorAttachments,
                },
                acquires
            );
            cmdList.SetViewport(fullViewport(dst, dstMip));
            cmdList.SetScissorRect(fullScissorRect(dst, dstMip));

            cmdList.SetPipelineState(pso);
            cmdList.SetPushGraphicsConstants(
                BloomParam{// a single-mip view, so Sample and GetDimensions in
                           // the shader see that mip and nothing else
                           .texture = src.GetReadableID(
                               RHITextureViewDesc{
                                   .format = src.GetFormat(),
                                   .mostDetailedMip = srcMip,
                                   .mipCount = 1
                               }
                           ),
                           .texelDir = texelDir,
                           .intensity = intensity
                }
            );
            cmdList.Draw(4);

            cmdList.EndRenderPass(releases);
        }
    };

    class BlackholeSimulation: public App {
        // the g^3 doppler boost drives the disk well past 1, which UNORM clips
        static constexpr auto HDR_FORMAT = RHIPixelFormat::RGBA16_FLOAT;
        static constexpr auto DISK_FORMAT = RHIPixelFormat::RGBA8_UNORM;

        RHIGraphicsPipelineStateRAII diskGenerator = nullptr;
        RHITextureRAII disk = nullptr;
        // procedural and never resized, so drawn once, by the first frame
        bool diskRecorded = false;
        struct GenerationParam {
            f32 rs;
            f32 diskInner;
            f32 diskOuter;
            f32 maxTempKelvin;
        };

        RHIGraphicsPipelineStateRAII blackholeSimulator = nullptr;
        RHITextureRAII scene = nullptr;
        struct PushConstants {
            u64 disk;
        };
        static constexpr u32 PLANET_COUNT = 3;
        struct SimulationParam {
            Vec3 bhPos{0.0, 0.0, 3.0f};
            f32 rs = 1.0f;
            Vec3 camPos{30.0f, 5.0f, 0.0f};
            f32 aspect;
            Vec3 camRight = unitX();
            f32 tanHalfFov =
                std::tanf(0.5f * static_cast<f32>(toRadian(45.0f)));
            Vec3 camUp = unitY();
            f32 diskInner = 3.0f;
            Vec3 camForward = unitZ();
            f32 diskOuter = 10.0f;
            Vec4 posRadius[PLANET_COUNT] = {
                Vec4{0, 0, 15, 2},
                Vec4{20, 0, -10, 2.5},
                Vec4{-15, 0, -15, 3}
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
        struct CompositeParam {
            u64 scene;
            u64 bloom;
            // brightMask carries the whole disk color, not the excess over the
            // threshold, so adding it at full strength would double the disk
            f32 bloomStrength = 0.2f;
        };

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override {
            diskGenerator = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader =
                        {.path = "Engine/RHI/Sample/BlackholeDisk.slang",
                         .entryPoint = "vs_main"},
                    .fragmentShader =
                        RHIShaderDesc{
                            .path = "Engine/RHI/Sample/BlackholeDisk.slang",
                            .entryPoint = "fs_main"
                        },
                    .renderTargetFormats = {DISK_FORMAT},
                    .renderTargetCount = 1
                }
            );
            disk = device.CreateTexture(
                RHITextureCreateDesc{
                    .width = swapchain.GetWidth(),
                    .height = swapchain.GetHeight(),
                    .format = DISK_FORMAT,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderRead
                    )
                }
            );

            blackholeSimulator = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader =
                        {.path = "Engine/RHI/Sample/Blackhole.slang",
                         .entryPoint = "vs_main"},
                    .fragmentShader =
                        RHIShaderDesc{
                            .path = "Engine/RHI/Sample/Blackhole.slang",
                            .entryPoint = "fs_main"
                        },
                    .renderTargetFormats = {HDR_FORMAT, HDR_FORMAT},
                    .renderTargetCount = 2
                }
            );

            simParam.camForward = normalize(simParam.bhPos - simParam.camPos);
            simParam.camRight = normalize(cross(unitY(), simParam.camForward));
            simParam.camUp = cross(simParam.camForward, simParam.camRight);

            createTargets(device, swapchain.GetWidth(), swapchain.GetHeight());
            bloomPass.Init(device, *brightMask);

            composite = device.CreatePipelineState(
                RHIGraphicsPipelineStateDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader =
                        {.path = "Engine/RHI/Sample/Composite.slang",
                         .entryPoint = "vs_main"},
                    .fragmentShader =
                        RHIShaderDesc{
                            .path = "Engine/RHI/Sample/Composite.slang",
                            .entryPoint = "fs_main"
                        },
                    .renderTargetFormats = {swapchain.GetFormat()},
                    .renderTargetCount = 1
                },
                "composite"
            );
        }

        void recordDisk(
            RHICommandList& cmdList,
            const RHITextureBarrier& release
        ) {
            std::array colorAttachments = {RHIColorAttachment{
                .texture = disk.get(),
                .loadAction = RHILoadAction::DontCare,
                .storeAction = RHIStoreAction::Store
            }};
            const std::array acquires{
                // first use of the freshly created texture
                makeBarrier(
                    *disk,
                    RHIResourceUsage::Undefined,
                    RHIResourceUsage::RenderTarget
                )
            };
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{
                    .colorAttachments = colorAttachments,
                },
                acquires
            );
            cmdList.SetViewport(fullViewport(*disk));
            cmdList.SetScissorRect(fullScissorRect(*disk));

            cmdList.SetPipelineState(*diskGenerator);
            cmdList.SetPushGraphicsConstants(
                GenerationParam{
                    .rs = simParam.rs,
                    .diskInner = simParam.diskInner,
                    .diskOuter = simParam.diskOuter,
                    .maxTempKelvin = 1e+4
                }
            );
            cmdList.Draw(4);

            const std::array releases{release};
            cmdList.EndRenderPass(releases);
        }

        void OnUpdate(f64, f64 elapsedTime) override {
            simParam.elapsedTimeSeconds = static_cast<f32>(elapsedTime);
        }

        void OnRecord(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override {
            simParamBuffer = Device().UploadTransient(simParam);

            // the simulation pass below is the disk's first reader
            const auto diskEdge = makeBarrier(
                *disk,
                RHIResourceUsage::RenderTarget,
                RHIResourceUsage::SampledFragment
            );
            const bool recordsDisk = !diskRecorded;
            if(recordsDisk) {
                recordDisk(cmdList, diskEdge);
                diskRecorded = true;
            }

            // released by the simulation pass, acquired only by the composite:
            // the non-adjacent dependency this barrier model exists for
            const auto sceneEdge = makeBarrier(
                *scene,
                RHIResourceUsage::RenderTarget,
                RHIResourceUsage::SampledFragment
            );
            // brightMask goes to the bloom's first blur instead (adjacent)
            const auto brightMaskEdge = makeBarrier(
                *brightMask,
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
                    makeCrossSubmissionBarrier(
                        *scene,
                        RHIResourceUsage::SampledFragment,
                        RHIResourceUsage::RenderTarget,
                        /*discardContents=*/true
                    ),
                    makeCrossSubmissionBarrier(
                        *brightMask,
                        RHIResourceUsage::SampledFragment,
                        RHIResourceUsage::RenderTarget,
                        /*discardContents=*/true
                    ),
                    // last, so the frames that did not draw the disk drop it
                    diskEdge
                };
                cmdList.BeginRenderPass(
                    RHIRenderPassDesc{.colorAttachments = colorAttachments},
                    std::span(acquires).first(recordsDisk ? 3 : 2)
                );
                cmdList.SetViewport(fullViewport(*backBuffer.texture));
                cmdList.SetScissorRect(fullScissorRect(*backBuffer.texture));

                cmdList.SetPipelineState(*blackholeSimulator);
                cmdList.SetPushGraphicsConstants(
                    PushConstants{.disk = disk->GetReadableID()}
                );
                cmdList.SetGraphicsConstantBuffer(simParamBuffer, 0);
                cmdList.Draw(4);

                const std::array releases{sceneEdge, brightMaskEdge};
                cmdList.EndRenderPass(releases);
            }

            RHITexture& bloom = bloomPass.Execute(*brightMask, cmdList);

            {
                std::array colorAttachments = {RHIColorAttachment{
                    .texture = backBuffer.texture,
                    // a full screen quad covers every pixel
                    .loadAction = RHILoadAction::DontCare,
                    .storeAction = backBuffer.storeAction
                }};
                const std::array acquires{
                    sceneEdge,
                    // the edge Execute's last upsample released
                    makeBarrier(
                        bloom,
                        RHIResourceUsage::RenderTarget,
                        RHIResourceUsage::SampledFragment
                    ),
                    AcquireBackBuffer(backBuffer)
                };
                cmdList.BeginRenderPass(
                    RHIRenderPassDesc{.colorAttachments = colorAttachments},
                    acquires
                );
                cmdList.SetViewport(fullViewport(*backBuffer.texture));
                cmdList.SetScissorRect(fullScissorRect(*backBuffer.texture));

                cmdList.SetPipelineState(*composite);
                cmdList.SetPushGraphicsConstants(
                    CompositeParam{
                        .scene = scene->GetReadableID(),
                        .bloom = bloom.GetReadableID()
                    }
                );
                cmdList.Draw(4);

                const std::array releases{ReleaseBackBuffer(backBuffer)};
                cmdList.EndRenderPass(releases);
            }
        }

        void OnResize(u32 width, u32 height) override {
            // a minimized window reports zero, which the swapchain also skips
            if(width == 0 || height == 0)
                return;

            // the OS waited for the GPU to go idle before calling this,
            // so the old targets can go right away
            createTargets(Device(), width, height);
        }

    private:
        // everything sized after the back buffer; the disk is not, since it
        // is sampled by its own coordinates and drawn only once
        void createTargets(RHIDevice& device, u32 width, u32 height) {
            const RHITextureCreateDesc desc{
                .width = width,
                .height = height,
                .format = HDR_FORMAT,
                .usage = combine(
                    RHITextureUsage::ShaderRead,
                    RHITextureUsage::RenderTarget
                )
            };
            scene = device.CreateTexture(desc, "scene");
            brightMask = device.CreateTexture(desc, "brightMask");
            bloomPass.CreateTargets(device, *brightMask, 4);

            simParam.aspect = static_cast<f32>(width) / height;
        }
    };
}

int main(int argc, char** argv) {
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "BlackholeSimulation",
        .width = 1280,
        .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<BlackholeSimulation>(argc, argv, windowConfig);
}
