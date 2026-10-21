#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <format>
#include <memory>
#include <vector>
#include "AppFramework.hpp"
#include "ClassRegistry.hpp"
#include "CommandPort.hpp"
#include "DOM.hpp"
#include "EnumUtil.hpp"
#include "FrameSelector.hpp"
#include "InputProvider.hpp"
#include "IntMath.hpp"
#include "LinearAlgebra.hpp"
#include "Log.hpp"
#include "Object.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

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
        void Init(RHIDevice& device, RHITexture& source) {
            RHIGraphicsPipelineStateDesc desc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/RHI/Sample/Bloom.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = RHIShaderDesc{
                    .path = "Engine/RHI/Sample/Bloom.slang",
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

    // angles are degrees throughout, so a port write reads the way the panel shows it
    struct PlanetOrbit{
        f32 orbitRadius = 15.0f;
        // from +x toward +z, before the tilt
        f32 orbitAngle = 0.0f;
        // degrees per second
        f32 orbitSpeed = 0.0f;
        // the orbit plane's tilt about the x axis
        f32 inclination = 0.0f;
        f32 radius = 2.0f;
        Color color{1, 1, 1, 1};
    };

    struct BlackholeControls{
        std::vector<PlanetOrbit> planets{
            PlanetOrbit{
                .orbitRadius = 15.0f, .orbitAngle = 90.0f, .orbitSpeed = 20.0f,
                .radius = 2.0f, .color = Color{0.5f, 0.1f, 0.1f, 1}
            },
            PlanetOrbit{
                .orbitRadius = 22.4f, .orbitAngle = -26.6f, .orbitSpeed = 12.0f,
                .radius = 2.5f, .color = Color{0.0f, 0.5f, 1.0f, 1}
            },
            PlanetOrbit{
                .orbitRadius = 21.2f, .orbitAngle = -135.0f, .orbitSpeed = 13.0f,
                .radius = 3.0f, .color = Color{0.4f, 0.7f, 0.1f, 1}
            }
        };
        // orbits the black hole the way OrbitCamera orbits its target
        f32 cameraYaw = 84.3f;
        f32 cameraPitch = 9.4f;
        f32 cameraDistance = 30.5f;
        f32 fovY = 45.0f;
        bool showDisk = true;
        bool paused = false;
        bool showPanel = true;
    };

    CROWY_STRUCT(PlanetOrbit)
        .SetProperty("orbitRadius", &PlanetOrbit::orbitRadius)
            .SetUIRange(4.0f, 60.0f)
        .SetProperty("orbitAngle", &PlanetOrbit::orbitAngle)
            .SetUIRange(-180.0f, 180.0f)
        .SetProperty("orbitSpeed", &PlanetOrbit::orbitSpeed)
            .SetUIRange(-90.0f, 90.0f)
        .SetProperty("inclination", &PlanetOrbit::inclination)
            .SetUIRange(-90.0f, 90.0f)
        .SetProperty("radius", &PlanetOrbit::radius)
            .SetUIRange(0.5f, 6.0f)
        .SetProperty("color", &PlanetOrbit::color)
    CROWY_STRUCT_END(PlanetOrbit)

    CROWY_STRUCT(BlackholeControls)
        .SetProperty("planets", &BlackholeControls::planets)
        .SetProperty("cameraYaw", &BlackholeControls::cameraYaw)
            .SetUIRange(-180.0f, 180.0f)
        .SetProperty("cameraPitch", &BlackholeControls::cameraPitch)
            .SetUIRange(-89.0f, 89.0f)
        .SetProperty("cameraDistance", &BlackholeControls::cameraDistance)
            .SetUIRange(6.0f, 90.0f)
        .SetProperty("fovY", &BlackholeControls::fovY)
            .SetUIRange(20.0f, 90.0f)
        .SetProperty("showDisk", &BlackholeControls::showDisk)
        .SetProperty("paused", &BlackholeControls::paused)
        .SetProperty("showPanel", &BlackholeControls::showPanel)
    CROWY_STRUCT_END(BlackholeControls)

    namespace{
        // travels the range the reflection registered, so the panel and the
        // port's describe agree on it
        template<typename T>
        void sliderFor(CStr property, f32& value, CStr format){
            const auto* desc = GetDesc<T>()->Find(property);
            CROWY_ASSERT(desc != nullptr && desc->meta.uiRange.has_value());
            const auto [min, max] = *desc->meta.uiRange;
            ImGui::SliderFloat(property, &value, min, max, format);
        }

        Str withFrame(StrView path, u64 frame){
            constexpr StrView FramePlaceholder = "{frame}";
            Str out(path);
            for(auto found = out.find(FramePlaceholder); found != Str::npos;
                found = out.find(FramePlaceholder, found)){
                const auto number = std::to_string(frame);
                out.replace(found, FramePlaceholder.size(), number);
                found += number.size();
            }
            return out;
        }
    }

    class BlackholeSimulation: public App{
        using App::App;

        // linear radiance. the g^3 doppler boost drives the disk well past 1,
        // which is exactly what an UNORM target used to throw away.
        static constexpr auto HDR_FORMAT = RHIPixelFormat::RGBA16_FLOAT;

        RHIGraphicsPipelineStateRAII diskGenerator = nullptr;
        RHITextureRAII disk = nullptr;
        // procedural and never resized, so drawn once, by the first frame
        bool diskRecorded = false;
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
            Vec3 camPos;
            f32 aspect;
            Vec3 camRight;
            f32 tanHalfFov;
            Vec3 camUp;
            f32 diskInner = 3.0f;
            Vec3 camForward;
            f32 diskOuter = 10.0f;
            // relative to the black hole
            Vec4 posRadius[PLANET_COUNT];
            Color planetColor[PLANET_COUNT];
            f32 elapsedTimeSeconds = 0.0f;
            f32 showDisk = 1.0f;
        } simParam;
        RHIBufferSlice simParamBuffer;

        BlackholeControls controls;
        RAII<UIRenderer> uiRenderer;

        // the verbs speak FramePort's protocol, so Tools/port.sh drives this
        // sample the way it drives a RenderApp
        RAII<CommandPort> port;
        // the port's captures go straight to it; it outlives every frame
        RHISwapchain* swapchain = nullptr;
        u64 captureFailures = 0;
        struct PendingWait{
            u64 frame;
            Reply reply;
        };
        std::vector<PendingWait> pendingWaits;
        // the disk's clock, which stops with the orbits
        f64 simulationSeconds = 0.0;

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
                        .path = "Engine/RHI/Sample/BlackholeDisk.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = RHIShaderDesc{
                    .path = "Engine/RHI/Sample/BlackholeDisk.slang",
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
                        .path = "Engine/RHI/Sample/Blackhole.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = RHIShaderDesc{
                    .path = "Engine/RHI/Sample/Blackhole.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    HDR_FORMAT,
                    HDR_FORMAT
                },
                .renderTargetCount = 2
            });

            createTargets(device, swapchain.GetWidth(), swapchain.GetHeight());
            bloomPass.Init(device, *brightMask);

            composite = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
                .preRasterizer = RHILegacyFrontendDesc{
                    .topology = RHIPrimitiveTopology::TriangleStrip,
                    .vertexShader = {
                        .path = "Engine/RHI/Sample/Composite.slang",
                        .entryPoint = "vs_main"
                    }
                },
                .rasterizer = RHIRasterizerState{
                    .frontCounterClockwise = false
                },
                .fragmentShader = RHIShaderDesc{
                    .path = "Engine/RHI/Sample/Composite.slang",
                    .entryPoint = "fs_main"
                },
                .renderTargetFormats = {
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            }, "composite");

            uiRenderer = std::make_unique<UIRenderer>(device, swapchain.GetFormat());

            this->swapchain = &swapchain;
#if defined(_DEBUG) || !defined(NDEBUG)
            openPort();
#endif
        }

        // called on held iterations too, so the port answers while the loop
        // waits for it
        void NewFrame() override{
            if(port == nullptr)
                return;

            for(const auto& outcome: swapchain->TakeFrameDumpOutcomes()){
                if(!outcome.written)
                    ++captureFailures;
            }
            answerWaits();
            port->Drain();
        }

        // the caller's first pass that samples the disk acquires `release`
        void recordDisk(RHICommandList& cmdList, const RHITextureBarrier& release){
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

            const std::array releases{release};
            cmdList.EndRenderPass(releases);
        }

        void ProcessInput(const InputProvider& input) override{
            constexpr f32 DegreesPerPixel = 0.3f;
            if(input.IsKeyDown(MouseButton::RButton)){
                const auto dpos = input.GetMouseDPos();
                controls.cameraYaw -= dpos.x * DegreesPerPixel;
                controls.cameraPitch += dpos.y * DegreesPerPixel;
            }
            // proportional, so a notch feels the same near and far
            controls.cameraDistance *= std::exp(-0.1f * input.GetMouseWheel());
        }

        void OnUpdate(f64 deltaTime, f64) override{
            if(!controls.paused){
                simulationSeconds += deltaTime;
                for(auto& planet: controls.planets){
                    planet.orbitAngle = std::remainder(
                        planet.orbitAngle + planet.orbitSpeed * static_cast<f32>(deltaTime),
                        360.0f
                    );
                }
            }
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            // before anything reads a control, so a slider dragged this frame
            // lands in this frame's picture
            if(controls.showPanel)
                drawPanel();
            // closes the ImGui frame even when no panel was drawn
            uiRenderer->Prepare(cmdList);

            applyControls();
            simParamBuffer = Device().UploadTransient(simParam);

            // the simulation pass below is the disk's first reader
            const auto diskEdge = MakeBarrier(*disk,
                RHIResourceUsage::RenderTarget,
                RHIResourceUsage::SampledFragment
            );
            const bool recordsDisk = !diskRecorded;
            if(recordsDisk){
                recordDisk(cmdList, diskEdge);
                diskRecorded = true;
            }

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
                    ),
                    // last, so the frames that did not draw the disk drop it
                    diskEdge
                };
                cmdList.BeginRenderPass(RHIRenderPassDesc{
                    .colorAttachments = colorAttachments
                }, std::span(acquires).first(recordsDisk ? 3 : 2));
                cmdList.SetViewport(FullViewport(*backBuffer.texture));
                cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

                cmdList.SetPipelineState(*blackholeSimulator);
                cmdList.SetPushGraphicsConstants(PushConstants{
                    // drawn once, by recordDisk in the first frame
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
                std::vector acquires{
                    sceneEdge,
                    // the edge Execute's last upsample released
                    MakeBarrier(bloom,
                        RHIResourceUsage::RenderTarget,
                        RHIResourceUsage::SampledFragment
                    ),
                    AcquireBackBuffer(backBuffer)
                };
                // the pass samples the font atlas Prepare just refreshed
                acquires.append_range(uiRenderer->TextureAcquires());
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

                uiRenderer->Record(cmdList);

                const std::array releases{ReleaseBackBuffer(backBuffer)};
                cmdList.EndRenderPass(releases);
            }
        }

        void OnResize(u32 width, u32 height) override{
            // a minimized window reports zero, which the swapchain also skips
            if(width == 0 || height == 0)
                return;

            // the OS waited for the GPU to go idle before calling this,
            // so the old targets can go right away
            createTargets(Device(), width, height);
        }

    private:
        // CROWY_COMMAND_PORT: unset is the default port with retries,
        // 0 disables, N forces exactly that port
        void openPort(){
            CommandPortConfig config;
            if(const char* env = std::getenv("CROWY_COMMAND_PORT")){
                const StrView text = env;
                u16 forced = 0;
                const auto [ptr, ec] =
                    std::from_chars(text.data(), text.data() + text.size(), forced);
                if(ec != std::errc{} || ptr != text.data() + text.size()){
                    LOG_WARN("BlackholeSimulation",
                        "CROWY_COMMAND_PORT='{}' is not a port number; using the default",
                        text
                    );
                }
                else if(forced == 0){
                    return;
                }
                else{
                    config.port = forced;
                    config.portRetries = 0;
                }
            }

            port = std::make_unique<CommandPort>(config);
            port->Expose("blackhole", &controls, *GetDesc<BlackholeControls>());
            registerVerbs();
            LOG_INFO("BlackholeSimulation", "command port on 127.0.0.1:{}", port->Port());
        }

        void registerVerbs(){
            port->RegisterVerb("ping", [this](const DOM::Value&, Reply reply){
                auto result = status();
                result.emplace("pong", DOM::Value(true));
                result.emplace("app", DOM::Value(Str("BlackholeSimulation")));
                result.emplace("capturesPending",
                    DOM::Value(static_cast<i64>(swapchain->PendingFrameDumps()))
                );
                result.emplace("captureFailures",
                    DOM::Value(static_cast<i64>(captureFailures))
                );
                reply.Ok(DOM::Value(std::move(result)));
            });
            port->RegisterVerb("quit", [this](const DOM::Value&, Reply reply){
                RequestQuit();
                reply.Ok(DOM::Value(DOM::Table{}));
            });
            port->RegisterVerb("run", [this](const DOM::Value& args, Reply reply){
                const auto* frames = args.at("frames");
                const auto* until = args.at("until");
                if(frames != nullptr && until != nullptr){
                    reply.Error("run takes frames or until, not both");
                    return;
                }

                if(frames != nullptr){
                    const auto count = parsePositiveInteger(frames);
                    if(!count){
                        reply.Error("frames must be a positive integer");
                        return;
                    }
                    Control().RunFrames(*count);
                }
                else if(until != nullptr){
                    const auto frame = parsePositiveInteger(until);
                    if(!frame){
                        reply.Error("until must be a positive integer");
                        return;
                    }
                    if(*frame <= FrameNumber()){
                        reply.Error(std::format(
                            "frame {} has already ended (the last frame is {})",
                            *frame, FrameNumber()
                        ));
                        return;
                    }
                    Control().RunUntil(*frame);
                }
                else{
                    Control().Run();
                }
                reply.Ok(DOM::Value(status()));
            });
            port->RegisterVerb("hold", [this](const DOM::Value&, Reply reply){
                Control().Hold();
                reply.Ok(DOM::Value(status()));
            });
            port->RegisterVerb("step", [this](const DOM::Value&, Reply reply){
                Control().RunFrames(1);
                reply.Ok(DOM::Value(status()));
            });
            // answers once `frame` has ended; it never advances the loop itself
            port->RegisterVerb("wait_frame", [this](const DOM::Value& args, Reply reply){
                const auto frame = parsePositiveInteger(args.at("frame"));
                if(!frame){
                    reply.Error("\"frame\" is missing or not a positive integer");
                    return;
                }
                if(*frame <= FrameNumber()){
                    reply.Ok(DOM::Value(status()));
                    return;
                }
                pendingWaits.push_back(PendingWait{.frame = *frame, .reply = std::move(reply)});
            });
            // the back buffer only; answers at once with what it queued, and
            // ping's capturesPending says when the files are on disk
            port->RegisterVerb("capture_frame", [this](const DOM::Value& args, Reply reply){
                const auto path = args.get<Str>("path");
                if(!path || path->empty()){
                    reply.Error("\"path\" is missing or not a string");
                    return;
                }
                if(args.at("target") != nullptr){
                    reply.Error("this sample captures the back buffer only");
                    return;
                }

                auto selector = parseFrameSelector(args);
                if(!selector.error.empty()){
                    reply.Error(selector.error);
                    return;
                }
                if(selector.frames.empty())
                    selector.frames.push_back(FrameNumber() + 1);
                const auto& frames = selector.frames;
                if(frames.size() > 1 && !path->contains("{frame}")){
                    reply.Error("path needs {frame} for more than one frame");
                    return;
                }

                // all or none: every frame is checked before any is queued
                const auto pending = swapchain->PendingFrameDumps();
                if(pending + frames.size() > MaxFrameDumps){
                    reply.Error(std::format("the capture queue is full ({} pending)", pending));
                    return;
                }
                std::vector<Str> paths;
                for(const auto frame: frames){
                    if(frame <= FrameNumber()){
                        reply.Error(std::format(
                            "frame {} has already been submitted (the last frame is {})",
                            frame, FrameNumber()
                        ));
                        return;
                    }
                    auto framePath = withFrame(*path, frame);
                    if(swapchain->IsFrameDumpQueued(frame, framePath)){
                        reply.Error(std::format(
                            "a capture for frame {} or to '{}' is already queued",
                            frame, framePath
                        ));
                        return;
                    }
                    paths.push_back(std::move(framePath));
                }

                DOM::Array queuedFrames;
                DOM::Array queuedPaths;
                for(usize i = 0; i < frames.size(); ++i){
                    swapchain->RequestFrameDump(paths[i], frames[i]);
                    queuedFrames.emplace_back(static_cast<i64>(frames[i]));
                    queuedPaths.emplace_back(paths[i]);
                }
                DOM::Table result;
                result.emplace("frames", DOM::Value(std::move(queuedFrames)));
                result.emplace("paths", DOM::Value(std::move(queuedPaths)));
                reply.Ok(DOM::Value(std::move(result)));
            });
        }

        // every control verb answers with where the loop stands afterwards
        DOM::Table status() const{
            const auto frame = FrameNumber();
            const auto holdAt = Control().HoldAt(frame);

            DOM::Table status;
            status.emplace("frame", DOM::Value(static_cast<i64>(frame)));
            status.emplace("held", DOM::Value(Control().IsHeld(frame)));
            status.emplace("holdAt",
                holdAt ? DOM::Value(static_cast<i64>(*holdAt)) : DOM::Value()
            );
            return status;
        }

        void answerWaits(){
            const auto frame = FrameNumber();
            for(auto& wait: pendingWaits){
                if(wait.frame <= frame)
                    wait.reply.Ok(DOM::Value(status()));
            }
            std::erase_if(pendingWaits, [frame](const PendingWait& wait){
                return wait.frame <= frame;
            });
        }

        void drawPanel(){
            ImGui::SetNextWindowPos(ImVec2(12.0f, 12.0f), ImGuiCond_FirstUseEver);
            ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f), ImGuiCond_FirstUseEver);
            ImGui::Begin("BlackholeSimulation");
            ImGui::PushItemWidth(-130.0f);

            ImGui::Checkbox("disk", &controls.showDisk);
            ImGui::SameLine();
            ImGui::Checkbox("paused", &controls.paused);

            if(ImGui::CollapsingHeader("camera", ImGuiTreeNodeFlags_DefaultOpen)){
                sliderFor<BlackholeControls>("cameraYaw", controls.cameraYaw, "%.1f deg");
                sliderFor<BlackholeControls>("cameraPitch", controls.cameraPitch, "%.1f deg");
                sliderFor<BlackholeControls>("cameraDistance", controls.cameraDistance, "%.1f");
                sliderFor<BlackholeControls>("fovY", controls.fovY, "%.0f deg");
                ImGui::TextDisabled("right drag orbits, the wheel zooms");
            }

            for(usize i = 0; i < controls.planets.size(); ++i){
                auto& planet = controls.planets[i];
                ImGui::PushID(static_cast<int>(i));
                const auto header = std::format("planet {}", i);
                if(ImGui::CollapsingHeader(header.c_str())){
                    sliderFor<PlanetOrbit>("orbitRadius", planet.orbitRadius, "%.1f");
                    sliderFor<PlanetOrbit>("orbitAngle", planet.orbitAngle, "%.1f deg");
                    sliderFor<PlanetOrbit>("orbitSpeed", planet.orbitSpeed, "%.1f deg/s");
                    sliderFor<PlanetOrbit>("inclination", planet.inclination, "%.1f deg");
                    sliderFor<PlanetOrbit>("radius", planet.radius, "%.2f");
                    ImGui::ColorEdit3("color", &planet.color.x);
                }
                ImGui::PopID();
            }

            ImGui::PopItemWidth();
            ImGui::End();
        }

        void applyControls(){
            // the shader marches no further than 100 rs, so a camera past it
            // would see nothing but the background
            controls.cameraPitch = std::clamp(controls.cameraPitch, -89.0f, 89.0f);
            controls.cameraDistance = std::clamp(controls.cameraDistance, 6.0f, 90.0f);

            const auto yaw = static_cast<f32>(toRadian(controls.cameraYaw));
            const auto pitch = static_cast<f32>(toRadian(controls.cameraPitch));
            const auto offset = controls.cameraDistance * Vec3{
                std::cos(pitch) * std::sin(yaw),
                std::sin(pitch),
                -std::cos(pitch) * std::cos(yaw)
            };
            simParam.camPos = simParam.bhPos + offset;
            simParam.camForward = normalize(-offset);
            simParam.camRight = normalize(cross(unitY(), simParam.camForward));
            simParam.camUp = cross(simParam.camForward, simParam.camRight);
            simParam.tanHalfFov = std::tan(0.5f * static_cast<f32>(toRadian(controls.fovY)));

            // the port can resize the list; a planet it no longer has is drawn
            // nowhere, since a zero radius never meets a step
            for(u32 i = 0; i < PLANET_COUNT; ++i){
                if(i >= controls.planets.size()){
                    simParam.posRadius[i] = Vec4{};
                    simParam.planetColor[i] = Color{};
                    continue;
                }

                const auto& planet = controls.planets[i];
                const auto angle = static_cast<f32>(toRadian(planet.orbitAngle));
                const auto tilt = static_cast<f32>(toRadian(planet.inclination));
                const auto x = planet.orbitRadius * std::cos(angle);
                const auto z = planet.orbitRadius * std::sin(angle);
                simParam.posRadius[i] = Vec4{
                    x, -z * std::sin(tilt), z * std::cos(tilt), planet.radius
                };
                simParam.planetColor[i] = planet.color;
            }

            simParam.elapsedTimeSeconds = static_cast<f32>(simulationSeconds);
            simParam.showDisk = controls.showDisk ? 1.0f : 0.0f;
        }

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
        .width = 1280, .height = 720,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<BlackholeSimulation>(argc, argv, windowConfig);
}
