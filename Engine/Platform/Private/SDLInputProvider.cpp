#include <utility>
#include "enum_traits.hpp"
#include <SDL3/SDL_keyboard.h>
#include "SDLInputProvider.hpp"

namespace Crowy
{
    static SDL_Scancode convert(KeyCode keyCode){
        switch(keyCode){
        // Numbers
        case KeyCode::Num0:    return SDL_SCANCODE_0;
        case KeyCode::Num1:    return SDL_SCANCODE_1;
        case KeyCode::Num2:    return SDL_SCANCODE_2;
        case KeyCode::Num3:    return SDL_SCANCODE_3;
        case KeyCode::Num4:    return SDL_SCANCODE_4;
        case KeyCode::Num5:    return SDL_SCANCODE_5;
        case KeyCode::Num6:    return SDL_SCANCODE_6;
        case KeyCode::Num7:    return SDL_SCANCODE_7;
        case KeyCode::Num8:    return SDL_SCANCODE_8;
        case KeyCode::Num9:    return SDL_SCANCODE_9;
        // Letters
        case KeyCode::A:       return SDL_SCANCODE_A;
        case KeyCode::B:       return SDL_SCANCODE_B;
        case KeyCode::C:       return SDL_SCANCODE_C;
        case KeyCode::D:       return SDL_SCANCODE_D;
        case KeyCode::E:       return SDL_SCANCODE_E;
        case KeyCode::F:       return SDL_SCANCODE_F;
        case KeyCode::G:       return SDL_SCANCODE_G;
        case KeyCode::H:       return SDL_SCANCODE_H;
        case KeyCode::I:       return SDL_SCANCODE_I;
        case KeyCode::J:       return SDL_SCANCODE_J;
        case KeyCode::K:       return SDL_SCANCODE_K;
        case KeyCode::L:       return SDL_SCANCODE_L;
        case KeyCode::M:       return SDL_SCANCODE_M;
        case KeyCode::N:       return SDL_SCANCODE_N;
        case KeyCode::O:       return SDL_SCANCODE_O;
        case KeyCode::P:       return SDL_SCANCODE_P;
        case KeyCode::Q:       return SDL_SCANCODE_Q;
        case KeyCode::R:       return SDL_SCANCODE_R;
        case KeyCode::S:       return SDL_SCANCODE_S;
        case KeyCode::T:       return SDL_SCANCODE_T;
        case KeyCode::U:       return SDL_SCANCODE_U;
        case KeyCode::V:       return SDL_SCANCODE_V;
        case KeyCode::W:       return SDL_SCANCODE_W;
        case KeyCode::X:       return SDL_SCANCODE_X;
        case KeyCode::Y:       return SDL_SCANCODE_Y;
        case KeyCode::Z:       return SDL_SCANCODE_Z;
        // Function Keys
        case KeyCode::F1:      return SDL_SCANCODE_F1;
        case KeyCode::F2:      return SDL_SCANCODE_F2;
        case KeyCode::F3:      return SDL_SCANCODE_F3;
        case KeyCode::F4:      return SDL_SCANCODE_F4;
        case KeyCode::F5:      return SDL_SCANCODE_F5;
        case KeyCode::F6:      return SDL_SCANCODE_F6;
        case KeyCode::F7:      return SDL_SCANCODE_F7;
        case KeyCode::F8:      return SDL_SCANCODE_F8;
        case KeyCode::F9:      return SDL_SCANCODE_F9;
        case KeyCode::F10:     return SDL_SCANCODE_F10;
        case KeyCode::F11:     return SDL_SCANCODE_F11;
        case KeyCode::F12:     return SDL_SCANCODE_F12;
        // Modifiers
        case KeyCode::Ctrl:    return SDL_SCANCODE_LCTRL;
        case KeyCode::Alt:     return SDL_SCANCODE_LALT;
        case KeyCode::Shift:   return SDL_SCANCODE_LSHIFT;
        // Special
        case KeyCode::Tab:     return SDL_SCANCODE_TAB;
        case KeyCode::Space:   return SDL_SCANCODE_SPACE;
        case KeyCode::Enter:   return SDL_SCANCODE_RETURN;
        case KeyCode::Escape:  return SDL_SCANCODE_ESCAPE;
        // Sentinel
        case KeyCode::Unknown: [[fallthrough]];
        case KeyCode::Count:   [[fallthrough]];
        default:
            std::unreachable();
        }
    }

    SDLInputProvider::SDLInputProvider()
        :transitionKeys(SDL_GetKeyboardState(nullptr))
    {}

    void SDLInputProvider::poll(){
        previousKeys = currentKeys;

        for(size_t i=0; i<NUM_KEY; ++i){
            auto sdlKey = convert(static_cast<KeyCode>(i));
            currentKeys[i] = transitionKeys[sdlKey];
        }
    }
}