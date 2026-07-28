#include <SDL3/SDL_init.h>
#include <SDL3/SDL_video.h>
#include <imgui_impl_sdl3.h>
#include "RuntimeConfig.hpp"
#include "SDLWindow.hpp"

namespace Crowy
{
    class SDLWindow::Impl{
    private:
        SDL_Window* window = nullptr;

    public:
        Impl(const WindowConfig&);
        ~Impl();

        void OnPlatformEvent(const SDL_WindowEvent&) noexcept;

        SDL_Window* GetWindow() const noexcept{ return window; }
        u32 GetWidth() const noexcept{
            int width = 0;
            SDL_GetWindowSize(window, &width, nullptr);
            return static_cast<u32>(width);
        }
        u32 GetHeight() const noexcept{
            int height = 0;
            SDL_GetWindowSize(window, nullptr, &height);
            return static_cast<u32>(height);
        }
        Size2D GetSize() const noexcept{
            int width = 0, height = 0;
            SDL_GetWindowSize(window, &width, &height);
            return{
                .x = static_cast<u32>(width),
                .y = static_cast<u32>(height)
            };
        }
    };

    SDLWindow::SDLWindow(const WindowConfig& config)
        : impl(config){}

    SDLWindow::Impl::Impl(const WindowConfig& config){
        SDL_InitFlags initFlags = SDL_INIT_VIDEO;
        if(!SDL_InitSubSystem(initFlags)){
            throw std::runtime_error(std::format(
                "Couldn't initialize SDL: {}",
                SDL_GetError()
            ));
        }

        SDL_WindowFlags windowFlags =
            (config.fullscreen    ? SDL_WINDOW_FULLSCREEN    : 0) |
            (config.resizable     ? SDL_WINDOW_RESIZABLE     : 0) |
            (config.borderless    ? SDL_WINDOW_BORDERLESS    : 0) |
            (config.always_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0);
        window = SDL_CreateWindow(
            config.title.c_str(),
            config.width,
            config.height,
            windowFlags
        );

        if(window == nullptr){
            SDL_QuitSubSystem(initFlags);

            throw std::runtime_error(std::format(
                "Couldn't create SDLWindow: {}",
                SDL_GetError()
            ));
        }
    }

    SDLWindow::~SDLWindow() = default;

    SDLWindow::Impl::~Impl(){
        if(window != nullptr){
            SDL_DestroyWindow(window);
            window = nullptr;

            SDL_QuitSubSystem(SDL_INIT_VIDEO);
        }
    }

    void SDLWindow::OnPlatformEvent(const SDL_WindowEvent& event) noexcept{
        impl->OnPlatformEvent(event);
    }

    void SDLWindow::Impl::OnPlatformEvent(const SDL_WindowEvent&) noexcept{
    }

    void* SDLWindow::GetWindow() const noexcept{ return impl->GetWindow(); }
    u32 SDLWindow::GetWidth() const noexcept{ return impl->GetWidth(); }
    u32 SDLWindow::GetHeight() const noexcept{ return impl->GetHeight(); }
    Size2D SDLWindow::GetSize() const noexcept{ return impl->GetSize(); }
}
