#include <utility>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include "KeyCode.hpp"
#include "SDLInputProvider.hpp"

namespace{
    SDL_Scancode convert(Crowy::KeyCode keyCode){
        using enum Crowy::KeyCode;

        switch(keyCode){
        // Numbers
        case Num0:    return SDL_SCANCODE_0;
        case Num1:    return SDL_SCANCODE_1;
        case Num2:    return SDL_SCANCODE_2;
        case Num3:    return SDL_SCANCODE_3;
        case Num4:    return SDL_SCANCODE_4;
        case Num5:    return SDL_SCANCODE_5;
        case Num6:    return SDL_SCANCODE_6;
        case Num7:    return SDL_SCANCODE_7;
        case Num8:    return SDL_SCANCODE_8;
        case Num9:    return SDL_SCANCODE_9;
        // Letters
        case A:       return SDL_SCANCODE_A;
        case B:       return SDL_SCANCODE_B;
        case C:       return SDL_SCANCODE_C;
        case D:       return SDL_SCANCODE_D;
        case E:       return SDL_SCANCODE_E;
        case F:       return SDL_SCANCODE_F;
        case G:       return SDL_SCANCODE_G;
        case H:       return SDL_SCANCODE_H;
        case I:       return SDL_SCANCODE_I;
        case J:       return SDL_SCANCODE_J;
        case K:       return SDL_SCANCODE_K;
        case L:       return SDL_SCANCODE_L;
        case M:       return SDL_SCANCODE_M;
        case N:       return SDL_SCANCODE_N;
        case O:       return SDL_SCANCODE_O;
        case P:       return SDL_SCANCODE_P;
        case Q:       return SDL_SCANCODE_Q;
        case R:       return SDL_SCANCODE_R;
        case S:       return SDL_SCANCODE_S;
        case T:       return SDL_SCANCODE_T;
        case U:       return SDL_SCANCODE_U;
        case V:       return SDL_SCANCODE_V;
        case W:       return SDL_SCANCODE_W;
        case X:       return SDL_SCANCODE_X;
        case Y:       return SDL_SCANCODE_Y;
        case Z:       return SDL_SCANCODE_Z;
        // Function Keys
        case F1:      return SDL_SCANCODE_F1;
        case F2:      return SDL_SCANCODE_F2;
        case F3:      return SDL_SCANCODE_F3;
        case F4:      return SDL_SCANCODE_F4;
        case F5:      return SDL_SCANCODE_F5;
        case F6:      return SDL_SCANCODE_F6;
        case F7:      return SDL_SCANCODE_F7;
        case F8:      return SDL_SCANCODE_F8;
        case F9:      return SDL_SCANCODE_F9;
        case F10:     return SDL_SCANCODE_F10;
        case F11:     return SDL_SCANCODE_F11;
        case F12:     return SDL_SCANCODE_F12;
        // Modifiers
        case Ctrl:    return SDL_SCANCODE_LCTRL;
        case Alt:     return SDL_SCANCODE_LALT;
        case Shift:   return SDL_SCANCODE_LSHIFT;
        // Special
        case Tab:     return SDL_SCANCODE_TAB;
        case Space:   return SDL_SCANCODE_SPACE;
        case Enter:   return SDL_SCANCODE_RETURN;
        case Escape:  return SDL_SCANCODE_ESCAPE;
        // Sentinel
        case Unknown: [[fallthrough]];
        default:
            std::unreachable();
        }
    }

    inline Crowy::KeyCode convert(SDL_Scancode scancode){
        using enum Crowy::KeyCode;

        switch(scancode){
        // Numbers
        case SDL_SCANCODE_0:      return Num0;
        case SDL_SCANCODE_1:      return Num1;
        case SDL_SCANCODE_2:      return Num2;
        case SDL_SCANCODE_3:      return Num3;
        case SDL_SCANCODE_4:      return Num4;
        case SDL_SCANCODE_5:      return Num5;
        case SDL_SCANCODE_6:      return Num6;
        case SDL_SCANCODE_7:      return Num7;
        case SDL_SCANCODE_8:      return Num8;
        case SDL_SCANCODE_9:      return Num9;
        // Letters
        case SDL_SCANCODE_A:      return A;
        case SDL_SCANCODE_B:      return B;
        case SDL_SCANCODE_C:      return C;
        case SDL_SCANCODE_D:      return D;
        case SDL_SCANCODE_E:      return E;
        case SDL_SCANCODE_F:      return F;
        case SDL_SCANCODE_G:      return G;
        case SDL_SCANCODE_H:      return H;
        case SDL_SCANCODE_I:      return I;
        case SDL_SCANCODE_J:      return J;
        case SDL_SCANCODE_K:      return K;
        case SDL_SCANCODE_L:      return L;
        case SDL_SCANCODE_M:      return M;
        case SDL_SCANCODE_N:      return N;
        case SDL_SCANCODE_O:      return O;
        case SDL_SCANCODE_P:      return P;
        case SDL_SCANCODE_Q:      return Q;
        case SDL_SCANCODE_R:      return R;
        case SDL_SCANCODE_S:      return S;
        case SDL_SCANCODE_T:      return T;
        case SDL_SCANCODE_U:      return U;
        case SDL_SCANCODE_V:      return V;
        case SDL_SCANCODE_W:      return W;
        case SDL_SCANCODE_X:      return X;
        case SDL_SCANCODE_Y:      return Y;
        case SDL_SCANCODE_Z:      return Z;
        // Function Keys
        case SDL_SCANCODE_F1:     return F1;
        case SDL_SCANCODE_F2:     return F2;
        case SDL_SCANCODE_F3:     return F3;
        case SDL_SCANCODE_F4:     return F4;
        case SDL_SCANCODE_F5:     return F5;
        case SDL_SCANCODE_F6:     return F6;
        case SDL_SCANCODE_F7:     return F7;
        case SDL_SCANCODE_F8:     return F8;
        case SDL_SCANCODE_F9:     return F9;
        case SDL_SCANCODE_F10:    return F10;
        case SDL_SCANCODE_F11:    return F11;
        case SDL_SCANCODE_F12:    return F12;
        // Modifiers
        case SDL_SCANCODE_LCTRL:  return Ctrl;
        case SDL_SCANCODE_LALT:   return Alt;
        case SDL_SCANCODE_LSHIFT: return Shift;
        // Special
        case SDL_SCANCODE_TAB:    return Tab;
        case SDL_SCANCODE_SPACE:  return Space;
        case SDL_SCANCODE_RETURN: return Enter;
        case SDL_SCANCODE_ESCAPE: return Escape;
        // Sentinel
        default:
            return Unknown;
        }
    }

    inline Crowy::MouseButton convert(Uint8 mouseIndex){
        using enum Crowy::MouseButton;

        switch(mouseIndex){
        case SDL_BUTTON_LEFT:  return LButton;
        case SDL_BUTTON_RIGHT: return RButton;
        default:
            return Unknown;
        }
    }
}

namespace Crowy
{
    SDLInputProvider::SDLInputProvider(){
        SDL_InitFlags flags = SDL_INIT_EVENTS;
        if(!SDL_InitSubSystem(flags)){
            throw std::runtime_error(std::format(
                "Couldn't initialize SDL: {}",
                SDL_GetError()
            ));
        }

        heldState = SDL_GetKeyboardState(nullptr);
        if(heldState == nullptr){
            SDL_QuitSubSystem(flags);

            throw std::runtime_error(std::format(
                "Couldn't get keyboard state: {}",
                SDL_GetError()
            ));
        }

        Vec2 mousePos;
        SDL_GetMouseState(&mousePos.x, &mousePos.y);

        SetCurrentMousePos(mousePos);
        // call twice for init mouse.dpos
        SetCurrentMousePos(mousePos);
    }

    SDLInputProvider::~SDLInputProvider(){
        if(heldState != nullptr){
            heldState = nullptr;

            SDL_QuitSubSystem(SDL_INIT_EVENTS);
        }
    }

    bool SDLInputProvider::IsKeyDown(KeyCode keyCode) const noexcept{
        auto sdlCode = convert(keyCode);

        return heldState[sdlCode];
    }

    bool SDLInputProvider::IsKeyDown(MouseButton button) const noexcept{
        auto ordKey = static_cast<usize>(button);

        return mouseHeldState.test(ordKey);
    }

    void SDLInputProvider::NewFrame() noexcept{
        InputProvider::ConsumeEdge();
        mouseHeldState.reset();

        Vec2 mousePos;
        auto buttons = SDL_GetMouseState(&mousePos.x, &mousePos.y);

        SetCurrentMousePos(mousePos);

        if(buttons & SDL_BUTTON_MASK(SDL_BUTTON_LEFT)){
            mouseHeldState.set(static_cast<usize>(MouseButton::LButton));
        }
        if(buttons & SDL_BUTTON_MASK(SDL_BUTTON_RIGHT)){
            mouseHeldState.set(static_cast<usize>(MouseButton::RButton));
        }
    }

    void SDLInputProvider::OnPlatformEvent(const SDL_KeyboardEvent& event) noexcept{
        switch(event.type){
        case SDL_EVENT_KEY_DOWN:
            [[fallthrough]];
        case SDL_EVENT_KEY_UP: {
            if(event.repeat)
                break;

            auto keyCode = convert(event.scancode);
            if(keyCode == KeyCode::Unknown)
                break;

            if(event.down)
                PressKey(keyCode);
            else
                ReleaseKey(keyCode);
        } break;
        default:
            // Input Provider should handle specific case
            std::unreachable();
        }
    }

    void SDLInputProvider::OnPlatformEvent(const SDL_MouseButtonEvent& event) noexcept{
        switch(event.type){
        case SDL_EVENT_MOUSE_MOTION:
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            [[fallthrough]];
        case SDL_EVENT_MOUSE_BUTTON_UP: {

            auto mouseButton = convert(event.button);
            if(mouseButton == MouseButton::Unknown)
                break;

            if(event.down)
                PressKey(mouseButton);
            else
                ReleaseKey(mouseButton);
        } break;
        default:
            std::unreachable();
        }
    }
}
