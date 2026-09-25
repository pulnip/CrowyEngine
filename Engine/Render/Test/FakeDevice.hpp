#pragma once

#include <exception>
#include <stdexcept>
#include <vector>

#include "Function.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

namespace Crowy
{
    // counts its own destruction, so a test can tell retired from destroyed
    class FakeGraphicsPipelineState final: public RHIGraphicsPipelineState {
    private:
        u32& destroyed;

    public:
        ~FakeGraphicsPipelineState() override { ++destroyed; }
        CROWY_DECLARE_PINNED(FakeGraphicsPipelineState)

        explicit FakeGraphicsPipelineState(u32& destroyed)
            : destroyed(destroyed) {}
    };

    // compiles nothing: graphics pipelines are counted fakes, and a
    // deferred retire waits in `deferred` until the test runs it
    class FakeDevice final: public RHIDevice {
    public:
        using Reclaims = std::vector<std::move_only_function<void()>>;

        u32 creates = 0;
        // the create that throws, counted from 1; 0 never throws
        u32 failAt = 0;
        u32 destroyed = 0;
        Reclaims deferred;

        RHIFrameScopeRAII CreateFrameScope() override { std::terminate(); }
        RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc&,
            StrView
        ) override {
            std::terminate();
        }
        RHITextureRAII CreateTexture(
            const RHITextureCreateDesc&,
            StrView
        ) override {
            std::terminate();
        }

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView
        ) override {
            if(++creates == failAt)
                throw std::runtime_error("fake compile error");

            return std::make_unique<FakeGraphicsPipelineState>(destroyed);
        }
        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView
        ) override {
            std::terminate();
        }

        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView
        ) override {
            std::terminate();
        }
        RHICommandListRAII CreateCommandList() override { std::terminate(); }

        void Submit(std::span<RHICommandList*>, u64) override {
            std::terminate();
        }
        void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&,
            u64
        ) override {
            std::terminate();
        }
        u64 GetCompletedFrame() const noexcept override { return 0; }
        void WaitFrame(u64) override { std::terminate(); }
        void WaitIdle() override { std::terminate(); }

        void DeferRetire(std::move_only_function<void()> reclaim) override {
            deferred.push_back(std::move(reclaim));
        }

        RHIBufferSlice AllocateTransient(u32, u32) override {
            std::terminate();
        }
        RHICapabilities GetCapabilities() const noexcept override {
            std::terminate();
        }

        void RunDeferred() {
            for(auto& reclaim: deferred)
                reclaim();
            deferred.clear();
        }
    };
}
