#include <SDL3/SDL_video.h>
#include "Window.hpp"

namespace Crowy
{
    struct Window::Impl{
        SDL_Window* window;

        Impl()
            :window(SDL_CreateWindow("Crowy", 800, 600, 0)){}

        ~Impl(){
            SDL_DestroyWindow(window);
        }
    };

    Window::Window()
        :impl(std::make_unique<Window::Impl>()){}

    Window::~Window(){}
}