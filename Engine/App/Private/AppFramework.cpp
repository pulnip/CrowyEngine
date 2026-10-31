#include "AppFramework.hpp"

#include "RHIDebugScope.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    void App::Render(CommandListPool& pool, RHISwapchain& swapchain) {
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
