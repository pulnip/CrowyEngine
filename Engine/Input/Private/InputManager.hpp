#pragma once

#include <bitset>
#include <memory>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "string.hpp"
#include "InputProvider.hpp"
#include "InputSpec.hpp"

namespace Crowy
{
    class InputManager{
    private:
        std::unique_ptr<InputProvider> provider;

        static InputManager* instance;
        friend void initInputModule(std::unique_ptr<InputProvider>);
        friend void deinitInputModule();

        using ActionMap = std::unordered_map<Action, InputBindings, StringHash, std::equal_to<>>;
        ActionMap actionMap;

    public:
        InputManager(std::unique_ptr<InputProvider> provider)
            : provider(std::move(provider)) {}
        ~InputManager() = default;
        DECLARE_PINNED(InputManager)

        inline static auto singleton(){ return instance; }

        void loadConfig(const InputSpec&);

        void pollInput();
        bool isDown(KeyCode) const;
        KeyState getKeyState(KeyCode) const;
        bool isNone(KeyCode) const;
        bool isPressed(KeyCode) const;
        bool isReleased(KeyCode) const;
        bool isHeld(KeyCode) const;
        bool isAction(std::string_view) const;
    };
}