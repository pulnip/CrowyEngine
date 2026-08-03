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
                .format = swapchain.GetFormat(),
                .usage = combine(RHITextureUsage::ShaderRead, RHITextureUsage::ShaderWrite)
            });
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            // the compute pass releases the checkerboard, the copy pass
            // acquires it - one edge, same value on both ends
            const auto checkerboardEdge = MakeBarrier(*checkerboard,
                RHIResourceUsage::StorageCompute,
                RHIResourceUsage::CopySrc
            );

            {
                const std::array acquires{
                    // the previous frame's copy still reads the texture (WAR);
                    // every texel is rewritten, so the contents can go
                    MakeCrossSubmissionBarrier(
                        *checkerboard,
                        RHIResourceUsage::CopySrc,
                        RHIResourceUsage::StorageCompute,
                        /*discardContents=*/true
                    )
                };
                cmdList.BeginComputePass(acquires);

                cmdList.SetPipelineState(*pso);
                cmdList.SetPushComputeConstants(PushConstants{
                    .texture = checkerboard->GetWritableID(),
                    .cellWidth = 64,
                    .cellHeight = 32
                });

                cmdList.Dispatch({W, H, 1});

                const std::array releases{checkerboardEdge};
                cmdList.EndComputePass(releases);
            }

            {
                const std::array acquires{
                    checkerboardEdge,
                    // the backbuffer is a copy target here, not a render target
                    MakeBarrier(*backBuffer.texture,
                        RHIResourceUsage::Undefined,
                        RHIResourceUsage::CopyDst
                    )
                };
                cmdList.BeginBlitPass(acquires);
                cmdList.Copy(
                    *checkerboard,
                    *backBuffer.texture
                );
                const std::array releases{
                    MakeBarrier(*backBuffer.texture,
                        RHIResourceUsage::CopyDst,
                        RHIResourceUsage::Present
                    )
                };
                cmdList.EndBlitPass(releases);
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
