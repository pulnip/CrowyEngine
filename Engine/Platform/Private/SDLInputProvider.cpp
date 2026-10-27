#include "SDLInputProvider.hpp"

#include <utility>
#include <variant>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

#include "KeyCode.hpp"
#include "VariantUtil.hpp"

namespace
{
    SDL_Scancode convert(Crowy::KeyCode keyCode) {
        using enum Crowy::KeyCode;

        switch(keyCode) {
        // Numbers
        case Num0:
            return SDL_SCANCODE_0;
        case Num1:
            return SDL_SCANCODE_1;
        case Num2:
            return SDL_SCANCODE_2;
        case Num3:
            return SDL_SCANCODE_3;
        case Num4:
            return SDL_SCANCODE_4;
        case Num5:
            return SDL_SCANCODE_5;
        case Num6:
            return SDL_SCANCODE_6;
        case Num7:
            return SDL_SCANCODE_7;
        case Num8:
            return SDL_SCANCODE_8;
        case Num9:
            return SDL_SCANCODE_9;
        // Letters
        case A:
            return SDL_SCANCODE_A;
        case B:
            return SDL_SCANCODE_B;
        case C:
            return SDL_SCANCODE_C;
        case D:
            return SDL_SCANCODE_D;
        case E:
            return SDL_SCANCODE_E;
        case F:
            return SDL_SCANCODE_F;
        case G:
            return SDL_SCANCODE_G;
        case H:
            return SDL_SCANCODE_H;
        case I:
            return SDL_SCANCODE_I;
        case J:
            return SDL_SCANCODE_J;
        case K:
            return SDL_SCANCODE_K;
        case L:
            return SDL_SCANCODE_L;
        case M:
            return SDL_SCANCODE_M;
        case N:
            return SDL_SCANCODE_N;
        case O:
            return SDL_SCANCODE_O;
        case P:
            return SDL_SCANCODE_P;
        case Q:
            return SDL_SCANCODE_Q;
        case R:
            return SDL_SCANCODE_R;
        case S:
            return SDL_SCANCODE_S;
        case T:
            return SDL_SCANCODE_T;
        case U:
            return SDL_SCANCODE_U;
        case V:
            return SDL_SCANCODE_V;
        case W:
            return SDL_SCANCODE_W;
        case X:
            return SDL_SCANCODE_X;
        case Y:
            return SDL_SCANCODE_Y;
        case Z:
            return SDL_SCANCODE_Z;
        // Function Keys
        case F1:
            return SDL_SCANCODE_F1;
        case F2:
            return SDL_SCANCODE_F2;
        case F3:
            return SDL_SCANCODE_F3;
        case F4:
            return SDL_SCANCODE_F4;
        case F5:
            return SDL_SCANCODE_F5;
        case F6:
            return SDL_SCANCODE_F6;
        case F7:
            return SDL_SCANCODE_F7;
        case F8:
            return SDL_SCANCODE_F8;
        case F9:
            return SDL_SCANCODE_F9;
        case F10:
            return SDL_SCANCODE_F10;
        case F11:
            return SDL_SCANCODE_F11;
        case F12:
            return SDL_SCANCODE_F12;
        // Modifiers
        case Ctrl:
            return SDL_SCANCODE_LCTRL;
        case Alt:
            return SDL_SCANCODE_LALT;
        case Shift:
            return SDL_SCANCODE_LSHIFT;
        // Special
        case Tab:
            return SDL_SCANCODE_TAB;
        case Space:
            return SDL_SCANCODE_SPACE;
        case Enter:
            return SDL_SCANCODE_RETURN;
        case Escape:
            return SDL_SCANCODE_ESCAPE;
        // Arrows
        case Left:
            return SDL_SCANCODE_LEFT;
        case Right:
            return SDL_SCANCODE_RIGHT;
        case Up:
            return SDL_SCANCODE_UP;
        case Down:
            return SDL_SCANCODE_DOWN;
        // Sentinel
        case Unknown:
            [[fallthrough]];
        default:
            std::unreachable();
        }
    }

    inline Crowy::KeyCode convert(SDL_Scancode scancode) {
        using enum Crowy::KeyCode;

        switch(scancode) {
        // Numbers
        case SDL_SCANCODE_0:
            return Num0;
        case SDL_SCANCODE_1:
            return Num1;
        case SDL_SCANCODE_2:
            return Num2;
        case SDL_SCANCODE_3:
            return Num3;
        case SDL_SCANCODE_4:
            return Num4;
        case SDL_SCANCODE_5:
            return Num5;
        case SDL_SCANCODE_6:
            return Num6;
        case SDL_SCANCODE_7:
            return Num7;
        case SDL_SCANCODE_8:
            return Num8;
        case SDL_SCANCODE_9:
            return Num9;
        // Letters
        case SDL_SCANCODE_A:
            return A;
        case SDL_SCANCODE_B:
            return B;
        case SDL_SCANCODE_C:
            return C;
        case SDL_SCANCODE_D:
            return D;
        case SDL_SCANCODE_E:
            return E;
        case SDL_SCANCODE_F:
            return F;
        case SDL_SCANCODE_G:
            return G;
        case SDL_SCANCODE_H:
            return H;
        case SDL_SCANCODE_I:
            return I;
        case SDL_SCANCODE_J:
            return J;
        case SDL_SCANCODE_K:
            return K;
        case SDL_SCANCODE_L:
            return L;
        case SDL_SCANCODE_M:
            return M;
        case SDL_SCANCODE_N:
            return N;
        case SDL_SCANCODE_O:
            return O;
        case SDL_SCANCODE_P:
            return P;
        case SDL_SCANCODE_Q:
            return Q;
        case SDL_SCANCODE_R:
            return R;
        case SDL_SCANCODE_S:
            return S;
        case SDL_SCANCODE_T:
            return T;
        case SDL_SCANCODE_U:
            return U;
        case SDL_SCANCODE_V:
            return V;
        case SDL_SCANCODE_W:
            return W;
        case SDL_SCANCODE_X:
            return X;
        case SDL_SCANCODE_Y:
            return Y;
        case SDL_SCANCODE_Z:
            return Z;
        // Function Keys
        case SDL_SCANCODE_F1:
            return F1;
        case SDL_SCANCODE_F2:
            return F2;
        case SDL_SCANCODE_F3:
            return F3;
        case SDL_SCANCODE_F4:
            return F4;
        case SDL_SCANCODE_F5:
            return F5;
        case SDL_SCANCODE_F6:
            return F6;
        case SDL_SCANCODE_F7:
            return F7;
        case SDL_SCANCODE_F8:
            return F8;
        case SDL_SCANCODE_F9:
            return F9;
        case SDL_SCANCODE_F10:
            return F10;
        case SDL_SCANCODE_F11:
            return F11;
        case SDL_SCANCODE_F12:
            return F12;
        // Modifiers
        case SDL_SCANCODE_LCTRL:
            return Ctrl;
        case SDL_SCANCODE_LALT:
            return Alt;
        case SDL_SCANCODE_LSHIFT:
            return Shift;
        // Special
        case SDL_SCANCODE_TAB:
            return Tab;
        case SDL_SCANCODE_SPACE:
            return Space;
        case SDL_SCANCODE_RETURN:
            return Enter;
        case SDL_SCANCODE_ESCAPE:
            return Escape;
        // Arrows
        case SDL_SCANCODE_LEFT:
            return Left;
        case SDL_SCANCODE_RIGHT:
            return Right;
        case SDL_SCANCODE_UP:
            return Up;
        case SDL_SCANCODE_DOWN:
            return Down;
        // Sentinel
        default:
            return Unknown;
        }
    }

    inline Crowy::MouseButton convert(Uint8 mouseIndex) {
        using enum Crowy::MouseButton;

        switch(mouseIndex) {
        case SDL_BUTTON_LEFT:
            return LButton;
        case SDL_BUTTON_RIGHT:
            return RButton;
        case SDL_BUTTON_MIDDLE:
            return MButton;
        default:
            return Unknown;
        }
    }

    Uint8 sdlButtonOf(Crowy::MouseButton button) {
        using enum Crowy::MouseButton;

        switch(button) {
        case LButton:
            return SDL_BUTTON_LEFT;
        case RButton:
            return SDL_BUTTON_RIGHT;
        case MButton:
            return SDL_BUTTON_MIDDLE;
        case Unknown:
            [[fallthrough]];
        default:
            std::unreachable();
        }
    }
}

namespace Crowy
{
    SDLInputProvider::SDLInputProvider(bool ownsCursor)
        : ownsCursor(ownsCursor) {
        SDL_InitFlags flags = SDL_INIT_EVENTS;
        if(!SDL_InitSubSystem(flags)) {
            throw std::runtime_error(
                std::format("Couldn't initialize SDL: {}", SDL_GetError())
            );
        }

        // the first motion event moves it from here
        Vec2 mousePos;
        SDL_GetMouseState(&mousePos.x, &mousePos.y);
        SetMouseMotion(mousePos, Vec2{0.0f, 0.0f});
    }

    SDLInputProvider::~SDLInputProvider() {
        SDL_QuitSubSystem(SDL_INIT_EVENTS);
    }

    bool SDLInputProvider::IsKeyDown(KeyCode keyCode) const noexcept {
        auto ordKey = static_cast<usize>(keyCode);

        return !gate.keyboard && keyHeldState.test(ordKey);
    }

    bool SDLInputProvider::IsKeyDown(MouseButton button) const noexcept {
        auto ordKey = static_cast<usize>(button);

        return !gate.mouse && mouseHeldState.test(ordKey);
    }

    void SDLInputProvider::NewFrame(InputGate gate) noexcept {
        InputProvider::ConsumeEdge();
        this->gate = gate;

        // the motion events add the frame's delta
        SetMouseMotion(GetMousePos(), Vec2{0.0f, 0.0f});
    }

    void SDLInputProvider::SetRelativeMouse(
        SDL_Window* window,
        bool relative,
        bool restore
    ) noexcept {
        if(relative == this->relative)
            return;
        if(!ownsCursor) {
            this->relative = relative;
            return;
        }

        if(!relative && restore) {
            // still relative, this only moves SDL's own position, which
            // leaving puts the cursor back at
            const auto hiddenAt = GetMousePos();
            SDL_WarpMouseInWindow(window, hiddenAt.x, hiddenAt.y);
        }
        if(SDL_SetWindowRelativeMouseMode(window, relative))
            this->relative = relative;
    }

    void SDLInputProvider::OnFocusLost(SDL_Window* window) noexcept {
        // the cursor stays wherever the other window has it
        SetRelativeMouse(window, false, false);
        mouseHeldState.reset();
    }

    SDL_Event SDLInputProvider::Synthesize(
        const InputEvent& input,
        SDL_WindowID window
    ) const noexcept {
        // zeroed: a stray `which` would read as a touch
        SDL_Event event{};
        const auto timestamp = SDL_GetTicksNS();
        const auto pos = GetMousePos();

        std::visit(
            overload{
                [&](const KeyInput& key) {
                    auto held = keyHeldState;
                    held.set(static_cast<usize>(key.key), key.down);
                    // as SDL sets it: the modifiers after this key
                    const auto mod = static_cast<SDL_Keymod>(
                        (held.test(static_cast<usize>(KeyCode::Ctrl))
                             ? SDL_KMOD_LCTRL
                             : 0) |
                        (held.test(static_cast<usize>(KeyCode::Shift))
                             ? SDL_KMOD_LSHIFT
                             : 0) |
                        (held.test(static_cast<usize>(KeyCode::Alt))
                             ? SDL_KMOD_LALT
                             : 0)
                    );

                    event.key.type =
                        key.down ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                    event.key.timestamp = timestamp;
                    event.key.windowID = window;
                    event.key.scancode = convert(key.key);
                    event.key.key = SDL_GetKeyFromScancode(
                        event.key.scancode,
                        SDL_KMOD_NONE,
                        true
                    );
                    event.key.mod = mod;
                    event.key.down = key.down;
                },
                [&](const ButtonInput& button) {
                    event.button.type = button.down
                                            ? SDL_EVENT_MOUSE_BUTTON_DOWN
                                            : SDL_EVENT_MOUSE_BUTTON_UP;
                    event.button.timestamp = timestamp;
                    event.button.windowID = window;
                    event.button.button = sdlButtonOf(button.button);
                    event.button.down = button.down;
                    event.button.clicks = 1;
                    event.button.x = pos.x;
                    event.button.y = pos.y;
                },
                [&](const MoveInput& move) {
                    event.motion.type = SDL_EVENT_MOUSE_MOTION;
                    event.motion.timestamp = timestamp;
                    event.motion.windowID = window;
                    event.motion.x = pos.x + move.by.x;
                    event.motion.y = pos.y + move.by.y;
                    event.motion.xrel = move.by.x;
                    event.motion.yrel = move.by.y;
                },
                [&](const MoveToInput& move) {
                    event.motion.type = SDL_EVENT_MOUSE_MOTION;
                    event.motion.timestamp = timestamp;
                    event.motion.windowID = window;
                    event.motion.x = move.at.x;
                    event.motion.y = move.at.y;
                    event.motion.xrel = move.at.x - pos.x;
                    event.motion.yrel = move.at.y - pos.y;
                },
                [&](const WheelInput& wheel) {
                    event.wheel.type = SDL_EVENT_MOUSE_WHEEL;
                    event.wheel.timestamp = timestamp;
                    event.wheel.windowID = window;
                    event.wheel.y = wheel.notches;
                    event.wheel.direction = SDL_MOUSEWHEEL_NORMAL;
                    event.wheel.mouse_x = pos.x;
                    event.wheel.mouse_y = pos.y;
                }
            },
            input
        );

        return event;
    }

    void SDLInputProvider::OnPlatformEvent(
        const SDL_KeyboardEvent& event
    ) noexcept {
        switch(event.type) {
        case SDL_EVENT_KEY_DOWN:
            [[fallthrough]];
        case SDL_EVENT_KEY_UP: {
            if(event.repeat)
                break;

            auto keyCode = convert(event.scancode);
            if(keyCode == KeyCode::Unknown)
                break;

            // held whatever the gate, which only hides it, so a release the
            // UI took still lets go
            keyHeldState.set(static_cast<usize>(keyCode), event.down);
            if(gate.keyboard)
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

    void SDLInputProvider::OnPlatformEvent(
        const SDL_MouseMotionEvent& event
    ) noexcept {
        // a relative look's position is SDL's, clamped to the window
        if(!relative)
            SetMousePos(Vec2{event.x, event.y});
        if(gate.mouse)
            return;

        // its own delta, which a window edge clamps in neither mode
        AddMouseMotion(Vec2{event.xrel, event.yrel});
    }

    void SDLInputProvider::OnPlatformEvent(
        const SDL_MouseButtonEvent& event
    ) noexcept {
        switch(event.type) {
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            [[fallthrough]];
        case SDL_EVENT_MOUSE_BUTTON_UP: {
            auto mouseButton = convert(event.button);
            if(mouseButton == MouseButton::Unknown)
                break;

            mouseHeldState.set(static_cast<usize>(mouseButton), event.down);
            if(gate.mouse)
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

    void SDLInputProvider::OnPlatformEvent(
        const SDL_MouseWheelEvent& event
    ) noexcept {
        if(gate.mouse)
            return;

        // a flipped wheel reports the other way round from how it turned
        const auto flipped = event.direction == SDL_MOUSEWHEEL_FLIPPED;
        AddMouseWheel(flipped ? -event.y : event.y);
    }
}
