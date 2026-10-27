#pragma once

#include <cstring>
#include <optional>
#include <span>
#include <type_traits>
#include <utility>

#include "Function.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using NativeDeviceHandle = void*;

    class RHIDevice {
    public:
        CROWY_DECLARE_INTERFACE(RHIDevice)

        virtual RHIFrameScopeRAII CreateFrameScope() = 0;

        virtual RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView name = {}
        ) = 0;
        virtual RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView name = {}
        ) = 0;

        virtual RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView name = {}
        ) = 0;

        virtual RHICommandListRAII CreateCommandList() = 0;

        // the batch is frame `frame`'s; frames arrive contiguously from 1, and
        // frame N only once frame N - FramesInFlight has completed
        virtual void Submit(std::span<RHICommandList*>, u64 frame) = 0;
        virtual void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&,
            u64 frame
        ) = 0;

        // the newest frame whose submission the GPU has finished; 0 before
        // the first
        virtual u64 GetCompletedFrame() const noexcept = 0;
        // block the CPU until frame `frame`'s submission has finished
        virtual void WaitFrame(u64 frame) = 0;
        // block until every submission so far has completed
        virtual void WaitIdle() = 0;

        // the union of frame `frame`'s command list runs; empty, never 0, until
        // it completes and is read back, after it ages out, and on an untimed
        // GPU
        virtual std::optional<f64> GetGPUFrameTime(u64 frame) const noexcept {
            return std::nullopt;
        }

        // runs `reclaim` once the batch about to be submitted next has
        // completed on the GPU
        virtual void DeferRetire(std::move_only_function<void()> reclaim) = 0;

        // A CPU-writable range that stays readable by the GPU until the
        // batch it is recorded into completes.
        virtual RHIBufferSlice AllocateTransient(u32 size, u32 align) = 0;
        virtual RHICapabilities GetCapabilities() const noexcept = 0;

        RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView name = {}
        );
        RHITextureRAII CreateTexture(
            const RHITextureCreateDesc& desc,
            StrView name = {}
        );

        template<typename T>
        void Retire(RAII<T> object) {
            if(object == nullptr)
                return;

            DeferRetire([object = std::move(object)] {});
        }

        // allocate a transient slice and fill it in one step
        template<typename T>
            requires(!std::is_pointer_v<T> && std::is_trivially_copyable_v<T>)
        RHIBufferSlice UploadTransient(const T& data, u32 align = CbAlign) {
            const auto slice =
                AllocateTransient(static_cast<u32>(sizeof(T)), align);
            std::memcpy(slice.cpuPtr, &data, sizeof(T));

            return slice;
        }

        template<typename T>
            requires std::is_trivially_copyable_v<T>
        RHIBufferSlice UploadTransient(
            std::span<const T> data,
            u32 align = CbAlign
        ) {
            const auto bytes = static_cast<u32>(data.size_bytes());
            const auto slice = AllocateTransient(bytes, align);
            std::memcpy(slice.cpuPtr, data.data(), bytes);

            return slice;
        }

    private:
        virtual RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc&,
            StrView name
        ) = 0;
        virtual RHITextureRAII createTexture(
            const RHITextureCreateDesc&,
            StrView name
        ) = 0;
    };

#if defined(_WIN32)
    RHIDeviceRAII createDevice(RHIBackend backend = RHIBackend::DirectX12);
#elif defined(__APPLE__)
    RHIDeviceRAII createDevice(RHIBackend backend = RHIBackend::Metal);
#else
    RHIDeviceRAII createDevice(RHIBackend backend = RHIBackend::WebGPU);
#endif
}
