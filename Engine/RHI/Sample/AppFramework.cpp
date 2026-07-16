#include "AppFramework.hpp"
#include "RHICommandList.hpp"
#include "RHITexture.hpp"
#include "RHIDefinitions.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"

#if defined(_WIN32)
extern "C" {
    __declspec(dllexport) extern const std::uint32_t D3D12SDKVersion = 619;
    __declspec(dllexport) extern const char* D3D12SDKPath = ".\\";
}
#endif

namespace Crowy
{
    App::App(const WindowConfig& windowConfig)
        : window(windowConfig)
        , swapchain(device->CreateSwapchain(RHISwapchainCreateDesc{
            .sdlWindow = window.GetWindow(),
            .bufferDesc = RHITextureCreateDesc{
                .width = windowConfig.width,
                .height = windowConfig.height,
                .format = RHIPixelFormat::RGBA8_UNORM_SRGB
            }
        }))
    {}

    int App::Run(){
        OnInit(*device);

        while(true){
            if(!pumpEvents()) [[unlikely]]
                break;

            timer.NewFrame();
            OnUpdate(
                timer.GetDeltaTime(),
                timer.GetElapsedTime()
            );

            renderFrame();
        }

        framePacer.WaitForIdle();

        return 0;
    }

    bool App::pumpEvents(){
        bool keepRunning = true;

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_EVENT_QUIT: [[unlikely]]
                keepRunning = false;
                break;
            }

            if(!keepRunning) [[unlikely]]
                break;

            if(SDL_EVENT_WINDOW_FIRST <= event.type && event.type <= SDL_EVENT_WINDOW_LAST){
                if(event.type == SDL_EVENT_WINDOW_RESIZED){
                    int w = event.window.data1;
                    int h = event.window.data2;

                    framePacer.WaitForIdle();
                    swapchain->Resize(w, h);
                }
                window.OnPlatformEvent(event.window);
            }
        }

        return keepRunning;
    }

    void App::renderFrame(){
        framePacer.BeginFrame();
        swapchain->AcquireNextImage();
        cmdListPool.BeginFrame();

        auto& cmdList = cmdListPool.Acquire();
        cmdList.Begin();
        cmdList.TransitionBarrier(
            swapchain->GetCurrentTexture(),
            RHIResourceUsage::RenderTarget
        );

        RHIColorAttachment backBuffer{
            .texture = &swapchain->GetCurrentTexture(),
            .loadAction = RHILoadAction::Clear,
            .storeAction = RHIStoreAction::Store,
            .clearColor = Colors::Black
        };
        OnRecord(cmdList, backBuffer);

        cmdList.TransitionBarrier(
            swapchain->GetCurrentTexture(),
            RHIResourceUsage::Present
        );
        cmdList.Close();

        cmdListPool.SubmitFrame();
        swapchain->Present();
        framePacer.EndFrame();

        swapchain->GetCurrentTexture().TransitionState(
            RHIBarrierAccess::NoAccess
        );
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
