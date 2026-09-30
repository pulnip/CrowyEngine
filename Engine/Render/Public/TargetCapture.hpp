#pragma once

#include <bit>
#include <span>
#include <vector>

#include "FramePipeline.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RHISwapchain.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using FrameDumpOutcomes = std::vector<FrameDumpOutcome>;
    using TargetCaptureRequests = std::vector<TargetCaptureRequest>;
    using TargetReadbacks = std::vector<TargetReadback>;

    // the formats convertCapture takes
    [[nodiscard]] inline constexpr bool isCapturable(RHIPixelFormat format) {
        using enum RHIPixelFormat;

        switch(format) {
        case RGBA8_UNORM:
            [[fallthrough]];
        case RGBA8_UNORM_SRGB:
            [[fallthrough]];
        case BGRA8_UNORM:
            [[fallthrough]];
        case BGRA8_UNORM_SRGB:
            [[fallthrough]];
        case RGBA16_FLOAT:
            [[fallthrough]];
        case D32_FLOAT:
            return true;
        default:
            return false;
        }
    }

    // IEEE binary16 to binary32, subnormals, infinities and NaN included
    inline constexpr f32 halfToFloat(u16 bits) {
        const u32 sign = static_cast<u32>(bits & 0x8000u) << 16;
        const u32 exponent = (bits >> 10) & 0x1Fu;
        const u32 mantissa = bits & 0x3FFu;

        if(exponent == 0x1Fu)
            return std::bit_cast<f32>(sign | 0x7F800000u | (mantissa << 13));
        if(exponent != 0) {
            return std::bit_cast<f32>(
                sign | ((exponent + 127 - 15) << 23) | (mantissa << 13)
            );
        }

        const auto magnitude = static_cast<f32>(mantissa) * 0x1p-24f;

        return sign != 0 ? -magnitude : magnitude;
    }

    // tight RGBA8 rows, top-down; false for a format it cannot convert
    bool convertCapture(
        RHIPixelFormat format,
        std::span<const u8> bytes,
        u32 width,
        u32 height,
        u32 rowPitch,
        std::vector<u8>& rgba
    );

    // Main thread only: the requests in frame order, the copies in flight,
    // and the outcomes not yet taken.
    class TargetCaptureQueue {
    private:
        RHIDevice& device;
        TargetCaptureRequests requests;
        TargetReadbacks inFlight;
        FrameDumpOutcomes outcomes;
        // a download and its conversion, reused across captures
        std::vector<u8> bytes;
        std::vector<u8> rgba;

    public:
        // logs the requests that never ran and retires the copies in flight
        ~TargetCaptureQueue();
        CROWY_DECLARE_PINNED(TargetCaptureQueue)

        explicit TargetCaptureQueue(RHIDevice& device);

        // whether that frame's target or that path is already taken; an
        // empty target asks about the path alone
        bool IsQueued(u64 frame, StrView target, StrView path) const noexcept;
        // the caller checks IsQueued and MaxFrameDumps first
        void Request(TargetCaptureRequest request);
        // requests, copies in flight and outcomes not yet taken: 0 only once
        // every outcome is in hand
        usize Pending() const noexcept;
        // every request due at or before `frame`, in frame order
        TargetCaptureRequests TakeDue(u64 frame);
        // an outcome that was never written, logged with why
        void Fail(const TargetCaptureRequest& request, StrView why);
        void AddInFlight(TargetReadback readback);
        // downloads, converts and writes every copy whose frame has
        // completed, and retires its buffer
        void Collect(u64 completedFrame);
        // the captures that finished since the last call, oldest first
        FrameDumpOutcomes TakeOutcomes();

    private:
        bool write(TargetReadback& readback);
    };
}
