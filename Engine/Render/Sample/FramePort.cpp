#include "FramePort.hpp"

#include <charconv>
#include <cstdlib>
#include <format>
#include <stdexcept>
#include <utility>

#include "DOM.hpp"
#include "FrameSelector.hpp"
#include "Log.hpp"

namespace Crowy
{
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
        port->RegisterVerb("ping", [this](const DOM::Value&, Reply reply) {
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
        port->RegisterVerb("quit", [this](const DOM::Value&, Reply reply) {
            loop.requestQuit();
            reply.Ok(DOM::Value(DOM::Table{}));
        });
        port->RegisterVerb("run", [this](const DOM::Value& args, Reply reply) {
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
                            "frame {} has already ended (the last frame is {})",
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
        });
        // the gate runs right after this drain, so the loop stops at the
        // frame that has just ended
        port->RegisterVerb("hold", [this](const DOM::Value&, Reply reply) {
            loop.control.Hold();
            reply.Ok(DOM::Value(status()));
        });
        port->RegisterVerb("step", [this](const DOM::Value&, Reply reply) {
            loop.control.RunFrames(1);
            reply.Ok(DOM::Value(status()));
        });
        // answers once `frame` has ended; it never advances the loop itself
        port->RegisterVerb(
            "wait_frame",
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
