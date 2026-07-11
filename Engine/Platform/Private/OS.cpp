#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <imgui_impl_sdl3.h>
#include "CommandListPool.hpp"
#include "FramePacer.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"
#include "RuntimeConfig.hpp"
#include "SDLInputProvider.hpp"
#include "SDLWindow.hpp"
#include "Timer.hpp"

namespace Crowy
{
    OS* OS::singleton = nullptr;

    class SDLInitializer final{
    public:
        SDLInitializer(const RuntimeConfig&);
        ~SDLInitializer() = default;
    };

    SDLInitializer::SDLInitializer(const RuntimeConfig& config){
        if(!SDL_SetAppMetadata(
            config.name.c_str(),
            config.version.c_str(),
            config.identifier.c_str()
        )){
            throw std::runtime_error(std::format(
                "Couldn't set runtime metadata: {}",
                SDL_GetError()
            ));
        }
    }

    class OS::Impl{
    private:
        SDLInitializer initializer;
        SDLWindow window;
        RHISwapchainRAII swapchain = nullptr;
        u32 width = 0, height = 0;

        SDLInputProvider inputProvider;

        // Non-stop timer
        Timer sysTimer;
        FramePacer framePacer;
        CommandListPool cmdListPool;

    public:
        Impl(const RuntimeConfig&, RHIDevice&);
        ~Impl();

        void Run(MainLoop&, RHIDevice&);

        const InputProvider& GetInputProvider() noexcept{
            return inputProvider;
        }
        const Window& GetWindow() noexcept{
            return window;
        }

    private:
        bool ProcessEvents();

        void BeginFrame(RHIDevice&);
        void EndFrame(RHIDevice&);
    };

    OS::OS(
        const RuntimeConfig& config,
        RHIDevice& device
    )
        : impl(std::make_unique<Impl>(config, device))
    {
        singleton = this;
    }

    OS::Impl::Impl(
        const RuntimeConfig& config,
        RHIDevice& device
    )
        : initializer(config)
        , window(config.window)
        , inputProvider()
        , framePacer(device)
        , cmdListPool(device)
    {
        RHITextureCreateDesc backBufferDesc{
            .width = config.window.width,
            .height = config.window.height,
            .format = RHIPixelFormat::RGBA8_UNORM
        };

        swapchain = device.CreateSwapchain(RHISwapchainCreateDesc{
            .sdlWindow = window.GetWindow(),
            .bufferDesc = backBufferDesc,
            // triple buffering
            .bufferCount = 3,
            .vsync = true,
            .allowTearing = false
        #if defined(_DEBUG) || !defined(NDEBUG)
            , .debugName = std::format("Swapchain for {}", config.window.title)
        #endif
        });
    }

    OS::~OS(){
        singleton = nullptr;
    }

    OS::Impl::~Impl(){

    }

    void OS::Run(MainLoop& mainLoop, RHIDevice& device){
        impl->Run(mainLoop, device);
    }

    void OS::Impl::Run(MainLoop& mainLoop, RHIDevice& device){
        if(!mainLoop.Initialize())
            return;

        sysTimer.Reset();

        auto& mainCmdList = device.GetMainCmdList();

        while(true){
            sysTimer.NewFrame();

            if(!ProcessEvents()) [[unlikely]]
                break;

            if(!mainLoop.Update()) [[unlikely]]
                break;

            BeginFrame(device);
            if(!mainLoop.Render(cmdListPool, *swapchain)) [[unlikely]]
                break;

            mainCmdList.Begin();
            // for Immediate draw of ImGui
            if(!mainLoop.RenderUI(mainCmdList, *swapchain)) [[unlikely]]
                break;
            swapchain->Present(mainCmdList);

            mainCmdList.Close();
            EndFrame(device);
        }

        mainLoop.Finalize();
    }

    bool OS::Impl::ProcessEvents(){
        bool keepRunning = true;

        inputProvider.NewFrame();

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            ImGui_ImplSDL3_ProcessEvent(&event);
            if(SDL_EVENT_QUIT == event.type) [[unlikely]]{
                keepRunning = false;
                break;
            }

            switch(event.type){
            // Keyboard Event
            case SDL_EVENT_KEY_DOWN:
                [[fallthrough]];
            case SDL_EVENT_KEY_UP:
                inputProvider.OnPlatformEvent(event.key);
                break;
            // Mouse Event
            case SDL_EVENT_MOUSE_MOTION:
                [[fallthrough]];
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                [[fallthrough]];
            case SDL_EVENT_MOUSE_BUTTON_UP:
                inputProvider.OnPlatformEvent(event.button);
                break;
            }

            if(!keepRunning) [[unlikely]]
                break;

            // Window Event
            if(SDL_EVENT_WINDOW_FIRST <= event.type && event.type <= SDL_EVENT_WINDOW_LAST){
                if(event.type == SDL_EVENT_WINDOW_RESIZED){
                    int w = event.window.data1;
                    int h = event.window.data2;
                    swapchain->Resize(w, h);
                }
                window.OnPlatformEvent(event.window);
            }
        }

        return keepRunning;
    }

    void OS::Impl::BeginFrame(RHIDevice& device){
        framePacer.BeginFrame();

        if(swapchain != nullptr) [[unlikely]]
            swapchain->AcquireNextImage();

        cmdListPool.BeginFrame();
    }

    void OS::Impl::EndFrame(RHIDevice& device){
        cmdListPool.SubmitFrame(
            framePacer.GetCurrentFence(),
            framePacer.GetNextFenceValue()
        );

        auto& mainCmdList = device.GetMainCmdList();
        device.Submit(mainCmdList);

        framePacer.EndFrame();
    }

    const InputProvider& OS::GetInputProvider() const noexcept{
        return impl->GetInputProvider();
    }

    const Window& OS::GetWindow() const noexcept{
        return impl->GetWindow();
    }
}
