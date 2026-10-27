#include "SDLInputProvider.hpp"

#include <algorithm>
#include <array>
#include <utility>
#include <variant>

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>

#include "Assert.hpp"
#include "KeyCode.hpp"
#include "VariantUtil.hpp"

namespace
{
    using Crowy::KeyCode;
    using Crowy::MouseButton;
    using Crowy::usize;

    struct KeyRow {
        KeyCode code;
        SDL_Scancode native;
    };

    struct ButtonRow {
        MouseButton code;
        Uint8 native;
    };

    constexpr std::array<KeyRow, Crowy::NUM_KEY> KeyRows{{
        {KeyCode::Num0, SDL_SCANCODE_0},
        {KeyCode::Num1, SDL_SCANCODE_1},
        {KeyCode::Num2, SDL_SCANCODE_2},
        {KeyCode::Num3, SDL_SCANCODE_3},
        {KeyCode::Num4, SDL_SCANCODE_4},
        {KeyCode::Num5, SDL_SCANCODE_5},
        {KeyCode::Num6, SDL_SCANCODE_6},
        {KeyCode::Num7, SDL_SCANCODE_7},
        {KeyCode::Num8, SDL_SCANCODE_8},
        {KeyCode::Num9, SDL_SCANCODE_9},
        {KeyCode::A, SDL_SCANCODE_A},
        {KeyCode::B, SDL_SCANCODE_B},
        {KeyCode::C, SDL_SCANCODE_C},
        {KeyCode::D, SDL_SCANCODE_D},
        {KeyCode::E, SDL_SCANCODE_E},
        {KeyCode::F, SDL_SCANCODE_F},
        {KeyCode::G, SDL_SCANCODE_G},
        {KeyCode::H, SDL_SCANCODE_H},
        {KeyCode::I, SDL_SCANCODE_I},
        {KeyCode::J, SDL_SCANCODE_J},
        {KeyCode::K, SDL_SCANCODE_K},
        {KeyCode::L, SDL_SCANCODE_L},
        {KeyCode::M, SDL_SCANCODE_M},
        {KeyCode::N, SDL_SCANCODE_N},
        {KeyCode::O, SDL_SCANCODE_O},
        {KeyCode::P, SDL_SCANCODE_P},
        {KeyCode::Q, SDL_SCANCODE_Q},
        {KeyCode::R, SDL_SCANCODE_R},
        {KeyCode::S, SDL_SCANCODE_S},
        {KeyCode::T, SDL_SCANCODE_T},
        {KeyCode::U, SDL_SCANCODE_U},
        {KeyCode::V, SDL_SCANCODE_V},
        {KeyCode::W, SDL_SCANCODE_W},
        {KeyCode::X, SDL_SCANCODE_X},
        {KeyCode::Y, SDL_SCANCODE_Y},
        {KeyCode::Z, SDL_SCANCODE_Z},
        {KeyCode::F1, SDL_SCANCODE_F1},
        {KeyCode::F2, SDL_SCANCODE_F2},
        {KeyCode::F3, SDL_SCANCODE_F3},
        {KeyCode::F4, SDL_SCANCODE_F4},
        {KeyCode::F5, SDL_SCANCODE_F5},
        {KeyCode::F6, SDL_SCANCODE_F6},
        {KeyCode::F7, SDL_SCANCODE_F7},
        {KeyCode::F8, SDL_SCANCODE_F8},
        {KeyCode::F9, SDL_SCANCODE_F9},
        {KeyCode::F10, SDL_SCANCODE_F10},
        {KeyCode::F11, SDL_SCANCODE_F11},
        {KeyCode::F12, SDL_SCANCODE_F12},
        {KeyCode::Ctrl, SDL_SCANCODE_LCTRL},
        {KeyCode::Alt, SDL_SCANCODE_LALT},
        {KeyCode::Shift, SDL_SCANCODE_LSHIFT},
        {KeyCode::Tab, SDL_SCANCODE_TAB},
        {KeyCode::Space, SDL_SCANCODE_SPACE},
        {KeyCode::Enter, SDL_SCANCODE_RETURN},
        {KeyCode::Escape, SDL_SCANCODE_ESCAPE},
        {KeyCode::Left, SDL_SCANCODE_LEFT},
        {KeyCode::Right, SDL_SCANCODE_RIGHT},
        {KeyCode::Up, SDL_SCANCODE_UP},
        {KeyCode::Down, SDL_SCANCODE_DOWN},
    }};

    constexpr std::array<ButtonRow, Crowy::NUM_MOUSE_BUTTON> ButtonRows{{
        {MouseButton::LButton, SDL_BUTTON_LEFT},
        {MouseButton::RButton, SDL_BUTTON_RIGHT},
        {MouseButton::MButton, SDL_BUTTON_MIDDLE},
    }};

    template<typename Row, usize N>
    consteval bool inEnumOrder(const std::array<Row, N>& rows) {
        for(usize i = 0; i < N; ++i) {
            if(static_cast<usize>(rows[i].code) != i)
                return false;
        }
        return true;
    }

    static_assert(inEnumOrder(KeyRows));
    static_assert(inEnumOrder(ButtonRows));

    template<typename Row, usize N>
    auto toNative(const std::array<Row, N>& rows, decltype(Row::code) code) {
        const auto index = static_cast<usize>(code);
        CROWY_ASSERT(index < N, "no native code for an Unknown");

        return rows[index].native;
    }

    template<typename Row, usize N>
    auto fromNative(
        const std::array<Row, N>& rows,
        decltype(Row::native) native
    ) {
        const auto row = std::ranges::find(rows, native, &Row::native);

        return row != rows.end() ? row->code : decltype(Row::code)::Unknown;
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
                    event.key.scancode = toNative(KeyRows, key.key);
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
                    event.button.button = toNative(ButtonRows, button.button);
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

            auto keyCode = fromNative(KeyRows, event.scancode);
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
            auto mouseButton = fromNative(ButtonRows, event.button);
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
