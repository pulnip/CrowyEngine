#include "FrameCaptures.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <string>
#include <utility>
#include <vector>

#include "DOM.hpp"
#include "FrameSelector.hpp"
#include "Log.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"

namespace Crowy
{
    namespace
    {
        constexpr StrView FramePlaceholder = "{frame}";

        // "captures/f-{frame}.bmp" names frame 60 "captures/f-60.bmp"
        Str withFrame(StrView path, u64 frame) {
            const auto number = std::to_string(frame);

            Str named;
            usize at = 0;
            for(auto found = path.find(FramePlaceholder);
                found != StrView::npos;
                found = path.find(FramePlaceholder, at)) {
                named += path.substr(at, found - at);
                named += number;
                at = found + FramePlaceholder.size();
            }
            named += path.substr(at);

            return named;
        }
    }

    FrameCaptures::FrameCaptures(
        RHIDevice& device,
        RHISwapchain& swapchain,
        const RAII<FramePipeline>& pipeline,
        LastFrame lastFrame
    )
        : device(device),
          swapchain(swapchain),
          pipeline(pipeline),
          lastFrame(std::move(lastFrame)),
          targets(device) {}

    void FrameCaptures::RegisterVerbs(CommandPort& port) {
        // answers at once with what it queued; ping's capturesPending says
        // when the files are on disk, and captureFailures whether one failed
        port.RegisterVerb(
            "capture_frame",
            withSelectorKeys({"path", "target"}),
            [this](const DOM::Value& args, Reply reply) {
                const auto path = args.get<Str>("path");
                if(!path || path->empty()) {
                    reply.Error("\"path\" is missing or not a string");

                    return;
                }
                // a target of the running pipeline by name; none is the back
                // buffer
                Str target;
                if(args.at("target") != nullptr) {
                    const auto named = args.get<Str>("target");
                    if(!named || named->empty()) {
                        reply.Error("\"target\" is not a string");

                        return;
                    }
                    if(auto refusal = refuseTarget(*named); !refusal.empty()) {
                        reply.Error(std::move(refusal));

                        return;
                    }
                    target = *named;
                }

                auto selector = parseFrameSelector(args);
                if(!selector.error.empty()) {
                    reply.Error(selector.error);

                    return;
                }
                if(selector.frames.empty())
                    selector.frames.push_back(lastFrame() + 1);

                const auto& frames = selector.frames;
                if(frames.size() > 1 && !path->contains(FramePlaceholder)) {
                    reply.Error("path needs {frame} for more than one frame");

                    return;
                }

                std::vector<Str> paths;
                paths.reserve(frames.size());
                for(const auto frame: frames)
                    paths.push_back(withFrame(*path, frame));
                // all or none: every frame is checked before any is queued
                if(auto refusal = refuse(target, frames, paths);
                   !refusal.empty()) {
                    reply.Error(std::move(refusal));

                    return;
                }
                queue(target, frames, paths);

                DOM::Array queuedFrames;
                DOM::Array queuedPaths;
                for(usize i = 0; i < frames.size(); ++i) {
                    queuedFrames.emplace_back(static_cast<i64>(frames[i]));
                    queuedPaths.emplace_back(paths[i]);
                }

                DOM::Table result;
                result.emplace("frames", DOM::Value(std::move(queuedFrames)));
                result.emplace("paths", DOM::Value(std::move(queuedPaths)));
                reply.Ok(DOM::Value(std::move(result)));
            }
        );
    }

    Str FrameCaptures::refuseTarget(StrView name) const {
        const auto id = pipeline->FindTarget(name);
        if(!id) {
            Str names;
            for(const auto& target: pipeline->Targets()) {
                if(!pipeline->FindTarget(target.name))
                    continue;
                if(!names.empty())
                    names += ", ";
                names += target.name;
            }

            return std::format(
                "unknown target '{}' (the pipeline has {})",
                name,
                names
            );
        }
        if(!isCapturable(pipeline->Targets()[*id - 1].format)) {
            return std::format(
                "target '{}' has a format a capture cannot convert",
                name
            );
        }

        return {};
    }

    Str FrameCaptures::Request(Str path, u64 frame) {
        const std::array frames{frame};
        const std::array paths{std::move(path)};
        if(auto refusal = refuse({}, frames, paths); !refusal.empty())
            return refusal;
        queue({}, frames, paths);

        return {};
    }

    Str FrameCaptures::refuse(
        StrView target,
        std::span<const u64> frames,
        std::span<const Str> paths
    ) const {
        for(const auto frame: frames) {
            if(frame <= lastFrame())
                return std::format(
                    "frame {} has already been submitted (the last frame is "
                    "{})",
                    frame,
                    lastFrame()
                );
        }

        const usize pending =
            target.empty() ? swapchain.PendingFrameDumps() : targets.Pending();
        if(pending + frames.size() > MaxFrameDumps)
            return std::format(
                "the capture queue is full ({} pending)",
                pending
            );
        for(usize i = 0; i < frames.size(); ++i) {
            if(!isQueued(frames[i], target, paths[i]))
                continue;

            const auto what =
                target.empty()
                    ? std::format("frame {}", frames[i])
                    : std::format("'{}' at frame {}", target, frames[i]);

            return std::format(
                "a capture for {} or to '{}' is already queued",
                what,
                paths[i]
            );
        }

        return {};
    }

    void FrameCaptures::queue(
        StrView target,
        std::span<const u64> frames,
        std::span<const Str> paths
    ) {
        for(usize i = 0; i < frames.size(); ++i) {
            if(target.empty()) {
                swapchain.RequestFrameDump(paths[i], frames[i]);
            }
            else {
                targets.Request(
                    TargetCaptureRequest{
                        .frame = frames[i],
                        .target = Str(target),
                        .path = paths[i]
                    }
                );
            }
        }
    }

    bool FrameCaptures::isQueued(
        u64 frame,
        StrView target,
        StrView path
    ) const {
        // no request is for frame 0, so that asks the swapchain about the
        // path alone
        if(target.empty()) {
            return swapchain.IsFrameDumpQueued(frame, path) ||
                   targets.IsQueued(frame, {}, path);
        }

        return targets.IsQueued(frame, target, path) ||
               swapchain.IsFrameDumpQueued(0, path);
    }

    // a request whose target the list lost in a rebuild fails; a second one
    // for a target already taken this frame waits for the next
    std::span<TargetReadback> FrameCaptures::TakeDue() {
        due.clear();
        for(auto& request: targets.TakeDue(lastFrame())) {
            const auto taken = std::ranges::any_of(
                due,
                [&request](const TargetReadback& readback) {
                    return readback.request.target == request.target;
                }
            );
            if(taken) {
                targets.Request(std::move(request));
                continue;
            }

            const auto id = pipeline->FindTarget(request.target);
            if(!id) {
                targets.Fail(request, "the pipeline has no such target");
                continue;
            }
            if(!isCapturable(pipeline->Targets()[*id - 1].format)) {
                targets.Fail(request, "its format cannot be converted");
                continue;
            }
            due.push_back(
                TargetReadback{.request = std::move(request), .target = *id}
            );
        }

        return due;
    }

    // a capture's reply went out when it was queued, so a failure reaches
    // the client through ping and the log
    void FrameCaptures::Collect() {
        for(const auto& outcome: swapchain.TakeFrameDumpOutcomes()) {
            if(outcome.written)
                continue;

            ++failures;
            LOG_WARN(
                "RenderApp",
                "capture of frame {} (presented {}) to '{}' failed",
                outcome.requested,
                outcome.presented,
                outcome.path
            );
        }

        // the fence is read live, so a held loop still collects; the queue
        // logs each failure with its target
        targets.Collect(device.GetCompletedFrame());
        for(const auto& outcome: targets.TakeOutcomes()) {
            if(!outcome.written)
                ++failures;
        }
    }

    void FrameCaptures::HandOff() {
        for(auto& readback: due)
            targets.AddInFlight(std::move(readback));
        due.clear();
    }

    usize FrameCaptures::Pending() const noexcept {
        return swapchain.PendingFrameDumps() + targets.Pending();
    }
}
