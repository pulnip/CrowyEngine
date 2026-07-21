#include "AppFramework.hpp"
#include "EnumUtil.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
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
                    swapchain.GetFormat()
                },
                .renderTargetCount = 1
            });

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
                        .storeAction = RHIStoreAction::Store,
                        .clearColor = Colors::Black
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

            cmdList.SetPipelineState(*blackholeSimulator);
            cmdList.SetPushGraphicsConstants(PushConstants{
                .disk = disk->GetReadableID()
            });
            cmdList.SetGraphicsConstantBuffer(*simParamBuffer, 0);
            cmdList.Draw(4);

            cmdList.EndRenderPass();
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
