#include "string.hpp"
#include <string_view>
#include <unordered_map>
#include "InputBinder.hpp"

namespace Crowy
{
    static KeyCode toKeyCode(std::string_view str){
        static std::unordered_map<std::string, KeyCode,
            StringHash, std::equal_to<>
        > text2KeyCode = {
            // Numbers
            {  "NUM0", KeyCode::Num0  },
            {  "NUM1", KeyCode::Num1  },
            {  "NUM2", KeyCode::Num2  },
            {  "NUM3", KeyCode::Num3  },
            {  "NUM4", KeyCode::Num4  },
            {  "NUM5", KeyCode::Num5  },
            {  "NUM6", KeyCode::Num6  },
            {  "NUM7", KeyCode::Num7  },
            {  "NUM8", KeyCode::Num8  },
            {  "NUM9", KeyCode::Num9  },
            // Letters
            {     "A", KeyCode::A     },
            {     "B", KeyCode::B     },
            {     "C", KeyCode::C     },
            {     "D", KeyCode::D     },
            {     "E", KeyCode::E     },
            {     "F", KeyCode::F     },
            {     "G", KeyCode::G     },
            {     "H", KeyCode::H     },
            {     "I", KeyCode::I     },
            {     "J", KeyCode::J     },
            {     "K", KeyCode::K     },
            {     "L", KeyCode::L     },
            {     "M", KeyCode::M     },
            {     "N", KeyCode::N     },
            {     "O", KeyCode::O     },
            {     "P", KeyCode::P     },
            {     "Q", KeyCode::Q     },
            {     "R", KeyCode::R     },
            {     "S", KeyCode::S     },
            {     "T", KeyCode::T     },
            {     "U", KeyCode::U     },
            {     "V", KeyCode::V     },
            {     "W", KeyCode::W     },
            {     "X", KeyCode::X     },
            {     "Y", KeyCode::Y     },
            {     "Z", KeyCode::Z     },
            // Function Keys
            {    "F1", KeyCode::F1    },
            {    "F2", KeyCode::F2    },
            {    "F3", KeyCode::F3    },
            {    "F4", KeyCode::F4    },
            {    "F5", KeyCode::F5    },
            {    "F6", KeyCode::F6    },
            {    "F7", KeyCode::F7    },
            {    "F8", KeyCode::F8    },
            {    "F9", KeyCode::F9    },
            {   "F10", KeyCode::F10   },
            {   "F11", KeyCode::F11   },
            {   "F12", KeyCode::F12   },
            // Modifiers
            {  "CTRL", KeyCode::Ctrl  },
            {   "ALT", KeyCode::Alt   },
            { "SHIFT", KeyCode::Shift },
            // Special
            {   "TAB", KeyCode::Tab   },
            { "SPACE", KeyCode::Space },
            { "ENTER", KeyCode::Enter },
            {"ESCAPE", KeyCode::Escape},
        };
        auto upper = toUpper(str);

        auto it = text2KeyCode.find(upper);
        if(it == text2KeyCode.end()){
            return KeyCode::Unknown;
        }
        return it->second;
    }

    static KeyState toKeyState(std::string_view str){
        static std::unordered_map<std::string, KeyState,
            StringHash, std::equal_to<>
        > str2KeyState = {
            {    "NONE", KeyState::None    },
            { "PRESSED", KeyState::Pressed },
            {"RELEASED", KeyState::Released},
            {    "HELD", KeyState::Held    },
        };
        auto upper = toUpper(str);

        auto it = str2KeyState.find(upper);
        if(it == str2KeyState.end()){
            return KeyState::Unknown;
        }
        return it->second;
    }

    void BindingsBinder::validateAndPlan(const ValueArena&,
        const VTable&, size_t, InputElementBindPlan&
    ){
        // No-op
    }

    void BindingsBinder::validateAndPlanArray(const ValueArena& arena,
        const VArray& array, size_t elmIndex, InputElementBindPlan& plan
    ){
        InputBindings bindings;

        for(size_t i: array.elements){
            auto table = std::get_if<VTable>(&arena.nodes[i]);
            if(!table)
                continue;

            auto device = readString(arena, *table, plan.errors, "device");
            if(!device)
                continue;

            if(*device == "keyboard"){
                auto keyCode = readString(arena, *table, plan.errors, "keyCode");
                auto keyState = readString(arena, *table, plan.errors, "keyState");

                if(!keyCode || !keyState)
                    continue;

                bindings.push_back(
                    KeyboardBinding{
                        .keyCode = toKeyCode(*keyCode),
                        .keyState = toKeyState(*keyState)
                    }
                );
            }
        }

        plan.allBindings.push_back({
            .spec = bindings,
            .index = elmIndex,
            .location = array.location
        });
    }

    void BindingsBinder::freeze(InputSpec& spec, InputElementBindPlan& plan){
        for(const auto& binding: plan.allBindings){
            auto& action = spec.actions[binding.index];

            action.bindings = binding.spec;
        }
    }

    InputBinderRegistry makeInputBinderRegistry(){
        InputBinderRegistry reg;
        reg.emplace("bindings", std::make_unique<BindingsBinder>());

        return reg;
    }
}