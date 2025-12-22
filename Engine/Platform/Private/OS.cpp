#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#include "OS.hpp"
#include "MainLoop.hpp"
#include "Logger.hpp"

namespace Crowy
{
    struct WindowConfig{
        const char* title = "CrowyEngine";
        int width = 1280;
        int height = 720;
        bool fullscreen = false;
        bool resizable = true;
        bool borderless = false;
        bool always_on_top = false;
    };

    OS* OS::os;

    struct OS::Impl{
        SDL_Window* window = nullptr;

        Impl(const WindowConfig& config)
            :window(SDL_CreateWindow(config.title,
                config.width, config.height,
                (config.fullscreen    ? SDL_WINDOW_FULLSCREEN    : 0) |
                (config.resizable     ? SDL_WINDOW_RESIZABLE     : 0) |
                (config.borderless    ? SDL_WINDOW_BORDERLESS    : 0) |
                (config.always_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0)
            )){}

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

        os = this;
    }

    OS::~OS(){
        os = nullptr;
    }

    void OS::run(){
        if(!mainLoop) return;

        forceQuit = false;
        mainLoop->initialize();

        auto lastTicks = SDL_GetTicks();

        while(!forceQuit){
            processEvents();

            auto ticks = SDL_GetTicks();
            auto ticks_epalsed = ticks - lastTicks;
            auto step = (float)ticks_epalsed / 1'000.0;

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
}