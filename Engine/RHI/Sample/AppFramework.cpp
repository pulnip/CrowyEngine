#include "AppFramework.hpp"
#include "RHIDebugScope.hpp"
#include "RHITexture.hpp"

#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) extern const std::uint32_t D3D12SDKVersion = 619;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\";
}
#endif

namespace Crowy
{
    void App::Render(CommandListPool& pool, RHISwapchain& swapchain){
        auto& cmdList = pool.Acquire();
        cmdList.Begin();
        RHIColorAttachment backBuffer{
            .texture = &swapchain.GetCurrentTexture(),
            .loadAction = RHILoadAction::Clear,
            .storeAction = RHIStoreAction::Store,
            .clearColor = Colors::Black
        };

        {
            // the sample owns its pass structure, so it also owns the
            // backbuffer barriers (AcquireBackBuffer / ReleaseBackBuffer)
            RHIEventScope event(cmdList, "Frame");
            OnRecord(cmdList, backBuffer);
        }

        cmdList.Close();
    }

}
