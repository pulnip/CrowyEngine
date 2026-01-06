#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#ifdef __APPLE__
    #include <SDL3/SDL_metal.h>
#endif
#include "Input.hpp"
#include "SDLInputProvider.hpp"
#include "Logger.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    struct WindowConfig{
        const char* title = "Crowy";
        int width = 800;
        int height = 600;
        bool fullscreen = false;
        bool resizable = true;
        bool borderless = false;
        bool always_on_top = false;
    };

    OS* OS::instance = nullptr;

    struct OS::Impl{
        SDL_Window* window = nullptr;
    #ifdef __APPLE__
        SDL_MetalView view;
    #endif

        std::unique_ptr<InputProvider> inputProvider;
        RHIDevicePtr device;
        RHISwapchainPtr swapchain;

        Impl(const WindowConfig& config)
            :window(SDL_CreateWindow(config.title,
                config.width, config.height,
                (config.fullscreen    ? SDL_WINDOW_FULLSCREEN    : 0) |
                (config.resizable     ? SDL_WINDOW_RESIZABLE     : 0) |
                (config.borderless    ? SDL_WINDOW_BORDERLESS    : 0) |
                (config.always_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0)
            ))
            ,view(SDL_Metal_CreateView(window))
            ,device(createDevice())
            ,inputProvider(std::make_unique<SDLInputProvider>()){}

        ~Impl(){
            if(window){
                SDL_DestroyWindow(window);
            }
        }
    };

    OS::OS(){
        if(!SDL_SetAppMetadata("Crowy", "1.0", "com.example.crowy")){
            throw;
        }
        if(!SDL_Init(SDL_INIT_VIDEO)){
            throw;
        }

        impl = std::make_unique<Impl>(WindowConfig{});

        instance = this;
    }

    OS::~OS(){
        instance = nullptr;
    }

    void OS::run(){
        if(!mainLoop) return;

        forceQuit = false;
        mainLoop->initialize();

        auto lastTicks = SDL_GetTicks();

        while(!forceQuit){
            processEvents();

            auto ticks = SDL_GetTicks();
            auto ticks_elapsed = ticks - lastTicks;
            auto step = static_cast<float>(ticks_elapsed) / 1'000.0;

            lastTicks = ticks;

            if(!mainLoop->update(step))
                break;
        }

        mainLoop->finalize();
    }

    void OS::processEvents(){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_EVENT_QUIT:
                forceQuit = true;
                break;
            case SDL_EVENT_KEY_DOWN:
                [[fallthrough]];
            case SDL_EVENT_KEY_UP:
                pollInput();
            }
        }
    }

    uint64_t OS::getTicks_ms(){
        return SDL_GetTicks();
    }

    uint64_t OS::getTicks_us(){
        return SDL_GetTicksNS() / 1'000;
    }

    uint64_t OS::getTicks_ns(){
        return SDL_GetTicksNS();
    }

    RHIDevice* OS::getDevice(){
        return impl->device.get();
    }

    InputProvider* OS::getInputProvider(){
        return impl->inputProvider.get();
    }
}