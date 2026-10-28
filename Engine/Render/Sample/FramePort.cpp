#include "FramePort.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cstdlib>
#include <expected>
#include <format>
#include <optional>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "DOM.hpp"
#include "EnumUtil.hpp"
#include "FrameSelector.hpp"
#include "Log.hpp"

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

            return DOM::Value(std::move(xy));
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
                    return std::unexpected(
                        std::format("does not take {}", key)
                    );
            }
            if(kinds.size() != 1)
                return std::unexpected(
                    "takes one of key, button, move, moveTo, wheel"
                );

            const auto kind = kinds.front();
            const bool pressable = kind == "key" || kind == "button";
            const auto* down = node.at("down");
            if(!pressable && down != nullptr)
                return std::unexpected(
                    std::format("{} does not take down", kind)
                );
            const auto pressed =
                down != nullptr ? down->asBool() : std::nullopt;
            if(pressable && !pressed)
                return std::unexpected(
                    std::format("{} needs down, true or false", kind)
                );

            const auto* value = node.at(kind);
            if(kind == "key") {
                const auto* name = value->asString();
                const auto key = name != nullptr ? enumFromName<KeyCode>(*name)
                                                 : std::nullopt;
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
                    return std::unexpected(
                        "button is not a mouse button's name"
                    );

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

    FramePort::FramePort(FrameLoop loop, const FrameCaptures& captures)
        : loop(std::move(loop)), captures(captures) {}

    // CROWY_COMMAND_PORT: unset is the default port with retries,
    // 0 disables, N forces exactly that port
    void FramePort::Open() {
        CommandPortConfig portConfig;
        if(const char* env = std::getenv("CROWY_COMMAND_PORT")) {
            const StrView text = env;
            u16 forced = 0;
            const auto [ptr, ec] =
                std::from_chars(text.data(), text.data() + text.size(), forced);
            if(ec != std::errc{} || ptr != text.data() + text.size()) {
                LOG_WARN(
                    "RenderApp",
                    "CROWY_COMMAND_PORT='{}' is not a port number; using the "
                    "default",
                    text
                );
            }
            else if(forced == 0) {
                return;
            }
            else {
                portConfig.port = forced;
                portConfig.portRetries = 0;
            }
        }

        port = std::make_unique<CommandPort>(portConfig);
    }

    void FramePort::RegisterVerbs() {
        port->RegisterVerb("ping", {}, [this](const DOM::Value&, Reply reply) {
            auto result = status();
            result.emplace("pong", DOM::Value(true));
            result.emplace("app", DOM::Value(loop.title));
            result.emplace("elapsed", DOM::Value(loop.elapsedSeconds()));
            result.emplace(
                "capturesPending",
                DOM::Value(static_cast<i64>(captures.Pending()))
            );
            result.emplace(
                "captureFailures",
                DOM::Value(static_cast<i64>(captures.Failures()))
            );
            reply.Ok(DOM::Value(std::move(result)));
        });
        port->RegisterVerb("quit", {}, [this](const DOM::Value&, Reply reply) {
            loop.requestQuit();
            reply.Ok(DOM::Value(DOM::Table{}));
        });
        port->RegisterVerb(
            "run",
            {"frames", "until"},
            [this](const DOM::Value& args, Reply reply) {
                const auto* frames = args.at("frames");
                const auto* until = args.at("until");
                if(frames != nullptr && until != nullptr) {
                    reply.Error("run takes frames or until, not both");

                    return;
                }

                if(frames != nullptr) {
                    const auto count = parsePositiveInteger(frames);
                    if(!count) {
                        reply.Error("frames must be a positive integer");

                        return;
                    }
                    loop.control.RunFrames(*count);
                }
                else if(until != nullptr) {
                    const auto frame = parsePositiveInteger(until);
                    if(!frame) {
                        reply.Error("until must be a positive integer");

                        return;
                    }
                    if(*frame <= loop.frame()) {
                        reply.Error(
                            std::format(
                                "frame {} has already ended (the last frame is "
                                "{})",
                                *frame,
                                loop.frame()
                            )
                        );

                        return;
                    }
                    loop.control.RunUntil(*frame);
                }
                else {
                    loop.control.Run();
                }

                reply.Ok(DOM::Value(status()));
            }
        );
        // the gate runs right after this drain, so the loop stops at the
        // frame that has just ended
        port->RegisterVerb("hold", {}, [this](const DOM::Value&, Reply reply) {
            loop.control.Hold();
            reply.Ok(DOM::Value(status()));
        });
        port->RegisterVerb("step", {}, [this](const DOM::Value&, Reply reply) {
            loop.control.RunFrames(1);
            reply.Ok(DOM::Value(status()));
        });
        // answers once `frame` has ended; it never advances the loop itself
        port->RegisterVerb(
            "wait_frame",
            {"frame"},
            [this](const DOM::Value& args, Reply reply) {
                const auto frame = parsePositiveInteger(args.at("frame"));
                if(!frame) {
                    reply.Error(
                        "\"frame\" is missing or not a positive integer"
                    );

                    return;
                }
                if(*frame <= loop.frame()) {
                    reply.Ok(DOM::Value(status()));

                    return;
                }

                pendingWaits.push_back(
                    PendingWait{.frame = *frame, .reply = std::move(reply)}
                );
            }
        );
        // in order, at the next frame that advances; a hold keeps them until
        // a run or a step lets one through
        port->RegisterVerb(
            "push_input",
            {"events"},
            [this](const DOM::Value& args, Reply reply) {
                if(!loop.pushInput) {
                    reply.Error("push_input needs --port-input");

                    return;
                }
                const auto* node = args.at("events");
                const auto* list = node != nullptr ? node->asArray() : nullptr;
                if(list == nullptr || list->empty()) {
                    reply.Error("events must be a non-empty array");

                    return;
                }

                // all or none
                std::vector<InputEvent> events;
                events.reserve(list->size());
                for(usize i = 0; i < list->size(); ++i) {
                    auto event = parseInputEvent((*list)[i]);
                    if(!event) {
                        reply.Error(
                            std::format("events[{}] {}", i, event.error())
                        );

                        return;
                    }
                    events.push_back(*event);
                }
                for(const auto& event: events)
                    loop.pushInput(event);

                auto result = status();
                result.emplace(
                    "queued",
                    DOM::Value(static_cast<i64>(events.size()))
                );
                result.emplace(
                    "queuedFor",
                    DOM::Value(static_cast<i64>(loop.frame() + 1))
                );
                reply.Ok(DOM::Value(std::move(result)));
            }
        );
        // the last frame's input as the app saw it: what the UI took reads up
        port->RegisterVerb(
            "read_input",
            {},
            [this](const DOM::Value&, Reply reply) {
                reply.Ok(DOM::Value(inputReport()));
            }
        );
    }

    // only a port verb can release the hold, so no port is no hold
    void FramePort::Hold() {
        if(port == nullptr || port->Port() == 0) {
            throw std::runtime_error(
                "--hold needs the command port "
                "(Debug builds, and a port that binds)"
            );
        }
        loop.control.Hold();
    }

    // a --port-input run hears nothing but push_input, so no port is no input
    void FramePort::CheckPortInput() const {
        if(loop.pushInput && (port == nullptr || port->Port() == 0)) {
            throw std::runtime_error(
                "--port-input needs the command port "
                "(Debug builds, and a port that binds)"
            );
        }
    }

    void FramePort::Drain() {
        answerWaits();
        port->Drain();
    }

    // every control verb answers with where the loop stands afterwards
    DOM::Table FramePort::status() const {
        const auto frame = loop.frame();
        const auto holdAt = loop.control.HoldAt(frame);

        DOM::Table status;
        status.emplace("frame", DOM::Value(static_cast<i64>(frame)));
        status.emplace("held", DOM::Value(loop.control.IsHeld(frame)));
        status.emplace(
            "holdAt",
            holdAt ? DOM::Value(static_cast<i64>(*holdAt)) : DOM::Value()
        );

        return status;
    }

    DOM::Table FramePort::inputReport() const {
        CROWY_ASSERT(loop.input != nullptr && loop.inputStatus);

        const auto& input = *loop.input;
        const auto inputStatus = loop.inputStatus();

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
        report.emplace("frame", DOM::Value(static_cast<i64>(loop.frame())));
        report.emplace("relativeMouse", DOM::Value(inputStatus.relativeMouse));
        report.emplace("ui", DOM::Value(std::move(ui)));
        report.emplace("mouse", DOM::Value(std::move(mouse)));
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

    void FramePort::answerWaits() {
        const auto frame = loop.frame();
        for(auto& wait: pendingWaits) {
            if(wait.frame <= frame)
                wait.reply.Ok(DOM::Value(status()));
        }
        std::erase_if(pendingWaits, [frame](const PendingWait& wait) {
            return wait.frame <= frame;
        });
    }
}
