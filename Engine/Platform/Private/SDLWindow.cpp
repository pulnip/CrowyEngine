#include "SDLWindow.hpp"

#include <cstdlib>
#include <format>
#include <stdexcept>

#include <SDL3/SDL_init.h>

#include "LogLocal.hpp"
#include "RuntimeConfig.hpp"
#include "StringUtil.hpp"

namespace
{
    // CROWY_WINDOW_DISPLAY: unset leaves the placement to SDL, N centres the
    // window on display N in SDL's order, 0 the primary
    int initialPosition() {
        const char* env = std::getenv("CROWY_WINDOW_DISPLAY");
        if(env == nullptr)
            return SDL_WINDOWPOS_UNDEFINED;

        const std::string_view text = env;
        const auto index = Crowy::parseNumber<int>(text);

        int count = 0;
        SDL_DisplayID* displays = SDL_GetDisplays(&count);
        const bool valid =
            index && displays != nullptr && *index >= 0 && *index < count;
        const int position =
            valid ? static_cast<int>(
                        SDL_WINDOWPOS_CENTERED_DISPLAY(displays[*index])
                    )
                  : SDL_WINDOWPOS_UNDEFINED;
        SDL_free(displays);

        if(!valid) {
            LOG_WARN(
                "CROWY_WINDOW_DISPLAY='{}' is not a display index below {}; "
                "using the default",
                text,
                count
            );
        }

        return position;
    }
}

namespace Crowy
{
    SDLWindow::~SDLWindow() {
        SDL_DestroyWindow(window);
        SDL_QuitSubSystem(SDL_INIT_VIDEO);
    }

    SDLWindow::SDLWindow(const WindowConfig& config) {
        if(!SDL_InitSubSystem(SDL_INIT_VIDEO)) {
            throw std::runtime_error(
                std::format("Couldn't initialize SDL: {}", SDL_GetError())
            );
        }

        SDL_WindowFlags windowFlags =
            (config.fullscreen ? SDL_WINDOW_FULLSCREEN : 0) |
            (config.resizable ? SDL_WINDOW_RESIZABLE : 0) |
            (config.borderless ? SDL_WINDOW_BORDERLESS : 0) |
            (config.always_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0) |
            (config.focusable ? 0 : SDL_WINDOW_NOT_FOCUSABLE);
        const auto position = initialPosition();
        const SDL_PropertiesID properties = SDL_CreateProperties();
        SDL_SetStringProperty(
            properties,
            SDL_PROP_WINDOW_CREATE_TITLE_STRING,
            config.title.c_str()
        );
        SDL_SetNumberProperty(
            properties,
            SDL_PROP_WINDOW_CREATE_X_NUMBER,
            position
        );
        SDL_SetNumberProperty(
            properties,
            SDL_PROP_WINDOW_CREATE_Y_NUMBER,
            position
        );
        SDL_SetNumberProperty(
            properties,
            SDL_PROP_WINDOW_CREATE_WIDTH_NUMBER,
            config.width
        );
        SDL_SetNumberProperty(
            properties,
            SDL_PROP_WINDOW_CREATE_HEIGHT_NUMBER,
            config.height
        );
        SDL_SetNumberProperty(
            properties,
            SDL_PROP_WINDOW_CREATE_FLAGS_NUMBER,
            windowFlags
        );
        window = SDL_CreateWindowWithProperties(properties);
        SDL_DestroyProperties(properties);

        if(window == nullptr) {
            SDL_QuitSubSystem(SDL_INIT_VIDEO);

            throw std::runtime_error(
                std::format("Couldn't create SDLWindow: {}", SDL_GetError())
            );
        }
    }
}
