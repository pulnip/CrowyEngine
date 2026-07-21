#include "AppFramework.hpp"
#include "LinearAlgebra.hpp"
#include "Primitives.hpp"
#include "RHIBuffer.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    class BlackholeSimulation: public App{
        using App::App;

        RHIGraphicsPipelineStateRAII pso;

        struct PushConstants{
            u64 disk;
        };

        static constexpr u32 PLANET_COUNT = 3;

        struct Param{
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
        } param;
        RHIBufferRAII paramBuffer = nullptr;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(RHIGraphicsPipelineStateDesc{
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

            param.camForward = normalize(param.bhPos - param.camPos);
            param.camRight   = normalize(cross(unitY(), param.camForward));
            param.camUp      = cross(param.camForward, param.camRight);

            param.aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();
            paramBuffer = device.CreateBuffer(RHIBufferCreateDesc{
                .size = sizeof(param),
                .usage = RHIBufferUsage::ConstantBuffer,
                .access = RHIMemoryAccess::CPUWrite,
                .initialData = &param
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
            // PushConstants pushConstants{
            // };
            // cmdList.SetPushGraphicsConstants(pushConstants);
            cmdList.SetGraphicsConstantBuffer(*paramBuffer, 0);
            cmdList.Draw(4);

            cmdList.EndRenderPass();
        }

        void OnResize(u32 width, u32 height) override{
            param.aspect = static_cast<f32>(width) / height;
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
