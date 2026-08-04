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
        RHIColorAttachment backBuffer{
            .texture = &swapchain.GetCurrentTexture(),
            .loadAction = RHILoadAction::Clear,
            .storeAction = RHIStoreAction::Store,
            .clearColor = Colors::Black
        };

        // the sample owns its pass structure, so it also owns the
        // backbuffer barriers (AcquireBackBuffer / ReleaseBackBuffer)
        OnRecord(cmdList, backBuffer);

        cmdList.Close();
    }

    RHIViewport FullViewport(const RHITexture& texture, u32 mipLevel){
        return RHIViewport{
            .x = 0, .y = 0,
            .width = static_cast<f32>(texture.GetWidth(mipLevel)),
            .height = static_cast<f32>(texture.GetHeight(mipLevel)),
            .minDepth = 0, .maxDepth = 1
        };
    }

    RHIScissorRect FullScissorRect(const RHITexture& texture, u32 mipLevel){
        return RHIScissorRect{
            .left = 0, .top = 0,
            .right = static_cast<i32>(texture.GetWidth(mipLevel)),
            .bottom = static_cast<i32>(texture.GetHeight(mipLevel))
        };
    }
}
