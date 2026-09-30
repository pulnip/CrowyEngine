#include "TargetCapture.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstring>
#include <utility>

#include "Assert.hpp"
#include "LogLocal.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIUtil.hpp"

namespace Crowy
{
    namespace
    {
        // mirror of the depth view's curve, DebugView.slang
        constexpr f32 DepthViewExponent = 100.0f;

        // calls convert(texel, out) on every texel, row by row
        template<typename Convert>
        void eachTexel(
            std::span<const u8> bytes,
            u32 width,
            u32 height,
            u32 rowPitch,
            u32 texelSize,
            std::vector<u8>& rgba,
            Convert convert
        ) {
            rgba.resize(usize{width} * height * 4);
            for(u32 y = 0; y < height; ++y) {
                const auto* texel = bytes.data() + usize{y} * rowPitch;
                auto* out = rgba.data() + usize{y} * width * 4;
                for(u32 x = 0; x < width; ++x) {
                    convert(texel, out);
                    texel += texelSize;
                    out += 4;
                }
            }
        }

        // [0, 1] to 0..255, rounded to nearest; NaN is 0
        constexpr u8 unorm8(f32 value) {
            if(!(value > 0.0f))
                return 0;
            if(value >= 1.0f)
                return 255;

            return static_cast<u8>(value * 255.0f + 0.5f);
        }
    }

    bool convertCapture(
        RHIPixelFormat format,
        std::span<const u8> bytes,
        u32 width,
        u32 height,
        u32 rowPitch,
        std::vector<u8>& rgba
    ) {
        using enum RHIPixelFormat;

        if(!isCapturable(format))
            return false;
        CROWY_ASSERT(
            width > 0 && height > 0 &&
                bytes.size() >=
                    usize{rowPitch} * (height - 1) + GetRowPitch(format, width),
            "the rows do not fit the bytes"
        );

        const auto run = [&](u32 texelSize, auto convert) {
            eachTexel(bytes, width, height, rowPitch, texelSize, rgba, convert);
        };
        switch(format) {
        case RGBA8_UNORM:
            [[fallthrough]];
        case RGBA8_UNORM_SRGB:
            run(4, [](const u8* in, u8* out) { std::memcpy(out, in, 4); });
            break;
        case BGRA8_UNORM:
            [[fallthrough]];
        case BGRA8_UNORM_SRGB:
            run(4, [](const u8* in, u8* out) {
                out[0] = in[2];
                out[1] = in[1];
                out[2] = in[0];
                out[3] = in[3];
            });
            break;
        case RGBA16_FLOAT:
            run(8, [](const u8* in, u8* out) {
                for(usize c = 0; c < 4; ++c) {
                    u16 bits = 0;
                    std::memcpy(&bits, in + c * sizeof(bits), sizeof(bits));
                    out[c] = unorm8(halfToFloat(bits));
                }
            });
            break;
        case D32_FLOAT:
            run(4, [](const u8* in, u8* out) {
                f32 depth = 0.0f;
                std::memcpy(&depth, in, sizeof(depth));
                const auto grey =
                    unorm8(1.0f - std::pow(depth, DepthViewExponent));
                out[0] = grey;
                out[1] = grey;
                out[2] = grey;
                out[3] = 255;
            });
            break;
        default:
            std::unreachable();
        }

        return true;
    }

    TargetCaptureQueue::~TargetCaptureQueue() {
        for(const auto& request: requests) {
            LOG_WARN(
                "capture of '{}' at frame {} to '{}' never ran",
                request.target,
                request.frame,
                request.path
            );
        }
        for(auto& readback: inFlight) {
            LOG_WARN(
                "capture of '{}' at frame {} to '{}' was still in flight at "
                "shutdown",
                readback.request.target,
                readback.request.frame,
                readback.request.path
            );
            device.Retire(std::move(readback.buffer));
        }
    }

    TargetCaptureQueue::TargetCaptureQueue(RHIDevice& device)
        : device(device) {}

    bool TargetCaptureQueue::IsQueued(
        u64 frame,
        StrView target,
        StrView path
    ) const noexcept {
        const auto taken = [&](const TargetCaptureRequest& request) {
            return (request.frame == frame && request.target == target) ||
                   request.path == path;
        };

        return std::ranges::any_of(requests, taken) ||
               std::ranges::any_of(inFlight, taken, &TargetReadback::request) ||
               std::ranges::contains(outcomes, path, &FrameDumpOutcome::path);
    }

    void TargetCaptureQueue::Request(TargetCaptureRequest request) {
        CROWY_ASSERT(
            !IsQueued(request.frame, request.target, request.path),
            "'{}' at frame {} or '{}' already has a capture queued",
            request.target,
            request.frame,
            request.path
        );
        CROWY_ASSERT(
            Pending() < MaxFrameDumps,
            "more than {} target captures pending",
            MaxFrameDumps
        );

        const auto at = std::ranges::upper_bound(
            requests,
            request.frame,
            {},
            &TargetCaptureRequest::frame
        );
        requests.insert(at, std::move(request));
    }

    usize TargetCaptureQueue::Pending() const noexcept {
        return requests.size() + inFlight.size() + outcomes.size();
    }

    TargetCaptureRequests TargetCaptureQueue::TakeDue(u64 frame) {
        // at or before, so a frame that recorded nothing passes its requests on
        const auto due = std::ranges::upper_bound(
            requests,
            frame,
            {},
            &TargetCaptureRequest::frame
        );
        TargetCaptureRequests taken(
            std::make_move_iterator(requests.begin()),
            std::make_move_iterator(due)
        );
        requests.erase(requests.begin(), due);

        return taken;
    }

    void TargetCaptureQueue::Fail(
        const TargetCaptureRequest& request,
        StrView why
    ) {
        LOG_WARN(
            "capture of '{}' at frame {} to '{}' failed: {}",
            request.target,
            request.frame,
            request.path,
            why
        );
        outcomes.push_back(
            FrameDumpOutcome{
                .requested = request.frame,
                .presented = request.frame,
                .path = request.path,
                .written = false
            }
        );
    }

    void TargetCaptureQueue::AddInFlight(TargetReadback readback) {
        CROWY_ASSERT(readback.buffer != nullptr, "Record made no copy");

        inFlight.push_back(std::move(readback));
    }

    void TargetCaptureQueue::Collect(u64 completedFrame) {
        TargetReadbacks waiting;
        for(auto& readback: inFlight) {
            if(readback.recorded > completedFrame) {
                waiting.push_back(std::move(readback));
                continue;
            }

            outcomes.push_back(
                FrameDumpOutcome{
                    .requested = readback.request.frame,
                    .presented = readback.recorded,
                    .path = readback.request.path,
                    .written = write(readback)
                }
            );
            device.Retire(std::move(readback.buffer));
        }
        inFlight = std::move(waiting);
    }

    FrameDumpOutcomes TargetCaptureQueue::TakeOutcomes() {
        return std::exchange(outcomes, {});
    }

    bool TargetCaptureQueue::write(TargetReadback& readback) {
        const auto start = std::chrono::steady_clock::now();
        const auto& request = readback.request;

        const auto size = readback.rowPitch * readback.height;
        bytes.resize(size);
        readback.buffer->Download(bytes.data(), size);
        if(!convertCapture(
               readback.format,
               bytes,
               readback.width,
               readback.height,
               readback.rowPitch,
               rgba
           )) {
            LOG_WARN(
                "capture of '{}' at frame {} to '{}' failed: format {} has no "
                "conversion",
                request.target,
                request.frame,
                request.path,
                static_cast<u32>(readback.format)
            );

            return false;
        }

        if(!WriteBMP(
               rgba.data(),
               usize{readback.width} * 4,
               readback.width,
               readback.height,
               /*bgra=*/false,
               request.path
           )) {
            LOG_WARN(
                "capture of '{}' at frame {} to '{}' failed: the file cannot "
                "be created",
                request.target,
                request.frame,
                request.path
            );

            return false;
        }

        const std::chrono::duration<f64, std::milli> took =
            std::chrono::steady_clock::now() - start;
        LOG_INFO(
            "capture: wrote {}x{} '{}' of frame {} to '{}' ({:.1f} ms)",
            readback.width,
            readback.height,
            request.target,
            readback.recorded,
            request.path,
            took.count()
        );

        return true;
    }
}
