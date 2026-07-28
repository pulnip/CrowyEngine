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
#include "RHITexture.hpp"
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

        bool imguiEnabled = false;

    public:
        Impl(const RuntimeConfig&, RHIDevice&);
        ~Impl() = default;

        void Run(MainLoop&, RHIDevice&);

        const InputProvider& GetInputProvider() const noexcept{
            return inputProvider;
        }
        const Window& GetWindow() const noexcept{
            return window;
        }

    private:
        bool ProcessEvents(MainLoop&);

        void BeginFrame(RHIDevice&);
        void EndFrame(RHIDevice&);
    };

    OS::OS(
        const RuntimeConfig& config,
        RHIDevice& device
    )
        : impl(config, device)
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
            .format = config.window.format
        };

        swapchain = device.CreateSwapchain(RHISwapchainCreateDesc{
            .sdlWindow = window.GetWindow(),
            .bufferDesc = backBufferDesc,
            // triple buffering
            .bufferCount = 3,
            .vsync = true,
            .allowTearing = false
        }, std::format("Swapchain for {}", config.window.title));
    }

    OS::~OS(){
        singleton = nullptr;
    }

    void OS::Run(MainLoop& mainLoop, RHIDevice& device){
        impl->Run(mainLoop, device);
    }

    void OS::Impl::Run(MainLoop& mainLoop, RHIDevice& device){
        mainLoop.OnInit(device, *swapchain);

        imguiEnabled = ImGui::GetCurrentContext() != nullptr;
        if(imguiEnabled){
            auto sdlWindow = static_cast<SDL_Window*>(window.GetWindow());

            // D3D, Metal, Vulkan, ... all same!
            ImGui_ImplSDL3_InitForOther(sdlWindow);
        }

        sysTimer.Reset();

        {
            framePacer.BeginFrame();
            cmdListPool.BeginFrame();

            mainLoop.RenderOnce(cmdListPool);

            auto cmdLists = cmdListPool.ExtractRecorded();
            framePacer.EndFrame(cmdLists);
        }

        while(true){
            sysTimer.NewFrame();

            if(!ProcessEvents(mainLoop)) [[unlikely]]
                break;
            mainLoop.ProcessInput(inputProvider);

            if(!mainLoop.Update()) [[unlikely]]
                break;

            BeginFrame(device);

            if(imguiEnabled){
                ImGui_ImplSDL3_NewFrame();
                ImGui::NewFrame();
            }
            mainLoop.Render(cmdListPool, *swapchain);

            EndFrame(device);
        }

        framePacer.WaitForIdle();

        if(imguiEnabled){
            ImGui_ImplSDL3_Shutdown();
        }

        mainLoop.Finalize();
    }

    bool OS::Impl::ProcessEvents(MainLoop& mainLoop){
        bool keepRunning = true;

        inputProvider.NewFrame();

        bool uiWantsMouse = false, uiWantsKeyboard = false;
        if(imguiEnabled){
            const auto& io = ImGui::GetIO();
            uiWantsMouse = io.WantCaptureMouse;
            uiWantsKeyboard = io.WantCaptureKeyboard;
        }

        SDL_Event event;
        while(SDL_PollEvent(&event)){
            if(imguiEnabled)
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
                if(!uiWantsKeyboard)
                    inputProvider.OnPlatformEvent(event.key);
                break;
            // Mouse Event
            case SDL_EVENT_MOUSE_MOTION:
                [[fallthrough]];
            case SDL_EVENT_MOUSE_BUTTON_DOWN:
                [[fallthrough]];
            case SDL_EVENT_MOUSE_BUTTON_UP:
                if(!uiWantsMouse)
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

                    framePacer.WaitForIdle();

                    swapchain->Resize(w, h);
                    mainLoop.OnResize(w, h);
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
        auto cmdLists = cmdListPool.ExtractRecorded();

        swapchain->GetCurrentTexture().TransitionState(
            RHIBarrierAccess::NoAccess
        );

        framePacer.EndFrame(
            cmdLists,
            *swapchain.get()
        );
    }

    const InputProvider& OS::GetInputProvider() const noexcept{
        return impl->GetInputProvider();
    }

    const Window& OS::GetWindow() const noexcept{
        return impl->GetWindow();
    }
}
