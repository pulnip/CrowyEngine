#include "FramePort.hpp"

#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Assert.hpp"
#include "DOM.hpp"
#include "FrameSelector.hpp"
#include "InputCodec.hpp"

namespace Crowy
{
    FramePort::FramePort(FrameLoop loop, const FrameCaptures& captures)
        : loop(std::move(loop)), captures(captures) {}

    void FramePort::Open() {
        if(const auto config = commandPortConfigFromEnvironment())
            port = std::make_unique<CommandPort>(*config);
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
            reply.Ok(std::move(result));
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
                reply.Ok(std::move(result));
            }
        );
        // the last frame's input as the app saw it: what the UI took reads up
        port->RegisterVerb(
            "read_input",
            {},
            [this](const DOM::Value&, Reply reply) {
                CROWY_ASSERT(loop.input != nullptr && loop.inputStatus);
                reply.Ok(
                    DOM::Value(inputReport(
                        loop.frame(),
                        *loop.input,
                        loop.inputStatus()
                    ))
                );
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
