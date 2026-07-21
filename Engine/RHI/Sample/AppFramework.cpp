#include "AppFramework.hpp"
#include "RHITexture.hpp"

#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) extern const std::uint32_t D3D12SDKVersion = 619;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\";
}
#endif

namespace Crowy
{
    void App::RenderOnce(CommandListPool& pool){
        auto& cmdList = pool.Acquire();
        cmdList.Begin();

        OnInitialRecord(cmdList);

        cmdList.Close();
    }

    void App::Render(CommandListPool& pool, RHISwapchain& swapchain){
        auto& cmdList = pool.Acquire();
        cmdList.Begin();
        cmdList.TransitionBarrier(
            swapchain.GetCurrentTexture(),
            RHIResourceUsage::RenderTarget
        );
        RHIColorAttachment backBuffer{
            .texture = &swapchain.GetCurrentTexture(),
            .loadAction = RHILoadAction::Clear,
            .storeAction = RHIStoreAction::Store,
            .clearColor = Colors::Black
        };

        OnRecord(cmdList, backBuffer);

        cmdList.TransitionBarrier(
            swapchain.GetCurrentTexture(),
            RHIResourceUsage::Present
        );
        cmdList.Close();
    }

    RHIViewport FullViewport(const RHITexture& texture){
        return RHIViewport{
            .x = 0, .y = 0,
            .width = static_cast<f32>(texture.GetWidth()),
            .height = static_cast<f32>(texture.GetHeight()),
            .minDepth = 0, .maxDepth = 1
        };
    }

    RHIScissorRect FullScissorRect(const RHITexture& texture){
        return RHIScissorRect{
            .left = 0, .top = 0,
            .right = static_cast<i32>(texture.GetWidth()),
            .bottom = static_cast<i32>(texture.GetHeight())
        };
    }
}
