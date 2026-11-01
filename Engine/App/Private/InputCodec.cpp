#include "InputCodec.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <optional>
#include <utility>
#include <vector>

#include "EnumUtil.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        // [x, y], window points
        std::optional<Vec2> pointOf(const DOM::Value* node) {
            const auto* list = node != nullptr ? node->asArray() : nullptr;
            if(list == nullptr || list->size() != 2)
                return std::nullopt;

            const auto x = (*list)[0].get<f32>();
            const auto y = (*list)[1].get<f32>();
            if(!x || !y)
                return std::nullopt;

            return Vec2{*x, *y};
        }

        DOM::Value pointValue(Vec2 point) {
            DOM::Array xy;
            xy.emplace_back(static_cast<f64>(point.x));
            xy.emplace_back(static_cast<f64>(point.y));

            return xy;
        }

        DOM::Array namesWhere(const auto& test) {
            DOM::Array names;
            for(const auto& entry: EnumTraits<KeyCode>::entries) {
                if(test(entry.value))
                    names.emplace_back(Str(entry.name));
            }
            for(const auto& entry: EnumTraits<MouseButton>::entries) {
                if(test(entry.value))
                    names.emplace_back(Str(entry.name));
            }

            return names;
        }
    }

    // the error reads after the event's place in the list
    std::expected<InputEvent, Str> parseInputEvent(const DOM::Value& node) {
        constexpr std::array<StrView, 5>
            Kinds{"key", "button", "move", "moveTo", "wheel"};

        const auto* table = node.asTable();
        if(table == nullptr)
            return std::unexpected("is not an object");

        std::vector<StrView> kinds;
        for(const auto& [key, value]: *table) {
            if(std::ranges::find(Kinds, key) != Kinds.end())
                kinds.push_back(key);
            else if(key != "down")
                return std::unexpected(std::format("does not take {}", key));
        }
        if(kinds.size() != 1)
            return std::unexpected(
                std::format("takes one of {}", join(Kinds, ", "))
            );

        const auto kind = kinds.front();
        const bool pressable = kind == "key" || kind == "button";
        const auto* down = node.at("down");
        if(!pressable && down != nullptr)
            return std::unexpected(std::format("{} does not take down", kind));
        const auto pressed = down != nullptr ? down->asBool() : std::nullopt;
        if(pressable && !pressed)
            return std::unexpected(
                std::format("{} needs down, true or false", kind)
            );

        const auto* value = node.at(kind);
        if(kind == "key") {
            const auto* name = value->asString();
            const auto key =
                name != nullptr ? enumFromName<KeyCode>(*name) : std::nullopt;
            if(!key)
                return std::unexpected("key is not a key's name");

            return KeyInput{.key = *key, .down = *pressed};
        }
        if(kind == "button") {
            const auto* name = value->asString();
            const auto button = name != nullptr
                                    ? enumFromName<MouseButton>(*name)
                                    : std::nullopt;
            if(!button)
                return std::unexpected("button is not a mouse button's name");

            return ButtonInput{.button = *button, .down = *pressed};
        }
        if(kind == "wheel") {
            const auto notches = value->get<f32>();
            if(!notches)
                return std::unexpected("wheel is not a number of notches");

            return WheelInput{.notches = *notches};
        }

        const auto point = pointOf(value);
        if(!point)
            return std::unexpected(std::format("{} is not [x, y]", kind));
        if(kind == "move")
            return MoveInput{.by = *point};

        return MoveToInput{.at = *point};
    }

    DOM::Table inputReport(
        u64 frame,
        const InputProvider& input,
        const InputStatus& inputStatus
    ) {
        DOM::Table ui;
        ui.emplace("mouse", DOM::Value(inputStatus.uiHasMouse));
        ui.emplace("keyboard", DOM::Value(inputStatus.uiHasKeyboard));

        DOM::Table mouse;
        mouse.emplace("pos", pointValue(input.GetMousePos()));
        mouse.emplace("delta", pointValue(input.GetMouseDPos()));
        mouse.emplace(
            "wheel",
            DOM::Value(static_cast<f64>(input.GetMouseWheel()))
        );

        DOM::Table report;
        report.emplace("frame", DOM::Value(static_cast<i64>(frame)));
        report.emplace("relativeMouse", DOM::Value(inputStatus.relativeMouse));
        report.emplace("ui", std::move(ui));
        report.emplace("mouse", std::move(mouse));
        report.emplace("held", DOM::Value(namesWhere([&](auto code) {
                           return input.IsKeyDown(code);
                       })));
        report.emplace("pressed", DOM::Value(namesWhere([&](auto code) {
                           return input.IsKeyPressed(code);
                       })));
        report.emplace("released", DOM::Value(namesWhere([&](auto code) {
                           return input.IsKeyReleased(code);
                       })));

        return report;
    }
}
