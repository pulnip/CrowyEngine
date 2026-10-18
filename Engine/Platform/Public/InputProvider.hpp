#pragma once

#include <bitset>
#include "KeyCode.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class InputProvider{
    private:
        // Keyboard State
        std::bitset<NUM_KEY> pressedState;
        std::bitset<NUM_KEY> releasedState;

        struct MouseState{
            Vec2 pos;
            Vec2 dpos{0.0f, 0.0f};
            // notches this frame, positive away from the user
            f32 wheel = 0.0f;

            std::bitset<NUM_MOUSE_BUTTON> pressedState;
            std::bitset<NUM_MOUSE_BUTTON> releasedState;
        } mouse;

    public:
        CROWY_DECLARE_INTERFACE(InputProvider)

        // Call After All Event Process Done
        virtual bool IsKeyDown(KeyCode) const noexcept = 0;

        // true if Rising Edge Exist
        bool IsKeyPressed(KeyCode keyCode) const noexcept{
            auto ordKey = static_cast<usize>(keyCode);

            return pressedState.test(ordKey);
        }
        // true if Falling Edge Exist
        bool IsKeyReleased(KeyCode keyCode) const noexcept{
            auto ordKey = static_cast<usize>(keyCode);

            return releasedState.test(ordKey);
        }

        virtual bool IsKeyDown(MouseButton) const noexcept = 0;

        bool IsKeyPressed(MouseButton button) const noexcept{
            auto ordKey = static_cast<usize>(button);

            return mouse.pressedState.test(ordKey);
        }
        bool IsKeyReleased(MouseButton button) const noexcept{
            auto ordKey = static_cast<usize>(button);

            return mouse.releasedState.test(ordKey);
        }

        Vec2 GetMousePos() const noexcept{
            return mouse.pos;
        }
        Vec2 GetMouseDPos() const noexcept{
            return mouse.dpos;
        }
        f32 GetMouseWheel() const noexcept{
            return mouse.wheel;
        }

    protected:
        void ConsumeEdge() noexcept{
            pressedState.reset();
            releasedState.reset();

            mouse.pressedState.reset();
            mouse.releasedState.reset();
            mouse.wheel = 0.0f;
        }

        void PressKey(KeyCode keyCode) noexcept{
            auto ordKey = static_cast<usize>(keyCode);

            pressedState.set(ordKey);
        }
        void ReleaseKey(KeyCode keyCode) noexcept{
            auto ordKey = static_cast<usize>(keyCode);

            releasedState.set(ordKey);
        }

        void PressKey(MouseButton button) noexcept{
            auto ordKey = static_cast<usize>(button);

            mouse.pressedState.set(ordKey);
        }
        void ReleaseKey(MouseButton button) noexcept{
            auto ordKey = static_cast<usize>(button);

            mouse.releasedState.set(ordKey);
        }

        void AddMouseWheel(f32 notches) noexcept{
            mouse.wheel += notches;
        }

        void SetCurrentMousePos(Vec2 pos) noexcept{
            SetMouseMotion(pos, pos - mouse.pos);
        }
        // the motion a frame reports, apart from where the mouse ended up
        void SetMouseMotion(Vec2 pos, Vec2 dpos) noexcept{
            mouse.dpos = dpos;
            mouse.pos = pos;
        }
        void AddMouseMotion(Vec2 delta) noexcept{
            mouse.dpos = mouse.dpos + delta;
        }
    };
}
