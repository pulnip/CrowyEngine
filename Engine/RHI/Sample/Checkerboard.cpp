#include "AppFramework.hpp"
#include "EnumUtil.hpp"
#include "RHICommandList.hpp"
#include "RHIPipelineState.hpp"

namespace{
    inline constexpr std::uint32_t W = 512, H = 512;
}

namespace Crowy
{
    class Checkerboard: public App{
        using App::App;

        RHIComputePipelineStateRAII pso;
        RHITextureRAII checkerboard;

        struct PushConstants{
            u64 texture;
            u32 cellWidth;
            u32 cellHeight;
        };

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            pso = device.CreatePipelineState(
                RHIComputePipelineStateDesc{
                    .computeShader = {
                        .path = "Engine/Shader/Checkerboard.slang",
                        .entryPoint = "cs_main"
                    }
                }
            );
            checkerboard = device.CreateTexture(RHITextureCreateDesc{
                .width = W, .height = H,
                .format = RHIPixelFormat::RGBA8_UNORM,
                .usage = combine(RHITextureUsage::ShaderRead, RHITextureUsage::ShaderWrite)
            });
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            cmdList.TransitionBarrier(*checkerboard,
                RHIResourceUsage::ComputeWrite
            );

            {
                cmdList.BeginCompute();

                cmdList.SetPipelineState(*pso);
                cmdList.SetPushComputeConstants(PushConstants{
                    .texture = checkerboard->GetWritableID(),
                    .cellWidth = 64,
                    .cellHeight = 32
                });

                cmdList.Dispatch({W, H, 1});

                cmdList.EndCompute();
            }

            std::array barriers = {
                MakeBarrier(*checkerboard, RHIResourceUsage::CopySrc),
                MakeBarrier(*backBuffer.texture, RHIResourceUsage::CopyDst)
            };
            cmdList.TransitionBarrier(barriers);

            {
                cmdList.BeginBlit();
                cmdList.Copy(
                    *checkerboard,
                    *backBuffer.texture
                );
                cmdList.EndBlit();
            }
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "Checkerboard",
        .width = W, .height = H,
        .format = RHIPixelFormat::RGBA8_UNORM,
        .fullscreen = false,
        .resizable = false,
    };
    return Main<Checkerboard>(windowConfig);
}
