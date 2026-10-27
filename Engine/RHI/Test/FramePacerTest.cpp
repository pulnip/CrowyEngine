#include <span>
#include <vector>

#include <gtest/gtest.h>

#include "FakeSwapchain.hpp"
#include "FramePacer.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIFrameScope.hpp"
#include "RHIPipelineState.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"

using namespace Crowy;

namespace
{
    // records what the pacer asks of the device and creates nothing, so the
    // pacer's own arithmetic is all that runs
    class PacerDevice final: public RHIDevice {
    public:
        std::vector<u64> waits;
        std::vector<u64> submits;

    public:
        RHIFrameScopeRAII CreateFrameScope() override { return nullptr; }

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc&,
            StrView
        ) override {
            return nullptr;
        }
        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc&,
            StrView
        ) override {
            return nullptr;
        }
        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView
        ) override {
            return nullptr;
        }
        RHICommandListRAII CreateCommandList() override { return nullptr; }

        void Submit(std::span<RHICommandList*>, u64 frame) override {
            submits.push_back(frame);
        }
        void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&,
            u64 frame
        ) override {
            submits.push_back(frame);
        }

        u64 GetCompletedFrame() const noexcept override {
            return submits.empty() ? 0 : submits.back();
        }
        void WaitFrame(u64 frame) override { waits.push_back(frame); }
        void WaitIdle() override {}

        void DeferRetire(std::move_only_function<void()>) override {}
        RHIBufferSlice AllocateTransient(u32, u32) override { return {}; }
        RHICapabilities GetCapabilities() const noexcept override { return {}; }

    private:
        RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc&,
            StrView
        ) override {
            return nullptr;
        }
        RHITextureRAII createTexture(
            const RHITextureCreateDesc&,
            StrView
        ) override {
            return nullptr;
        }
    };

    void runFrames(FramePacer& pacer, FakeSwapchain& swapchain, u64 count) {
        for(u64 i = 0; i < count; ++i) {
            pacer.BeginFrame();
            pacer.EndFrame({}, swapchain);
        }
    }
}

TEST(FramePacer, FramesCountFromOne) {
    PacerDevice device;
    FakeSwapchain swapchain;
    FramePacer pacer(device);

    EXPECT_EQ(pacer.CurrentFrame(), 0u);
    EXPECT_EQ(pacer.BeginFrame(), 1u);
    EXPECT_EQ(pacer.CurrentFrame(), 1u);
    pacer.EndFrame({}, swapchain);
    EXPECT_EQ(pacer.BeginFrame(), 2u);
}

TEST(FramePacer, TheFirstFramesInFlightWaitForNothing) {
    PacerDevice device;
    FakeSwapchain swapchain;
    FramePacer pacer(device);

    runFrames(pacer, swapchain, FramesInFlight);

    EXPECT_TRUE(device.waits.empty());
}

TEST(FramePacer, FrameNWaitsForFrameNMinusFramesInFlight) {
    PacerDevice device;
    FakeSwapchain swapchain;
    FramePacer pacer(device);

    runFrames(pacer, swapchain, FramesInFlight + 3);

    const std::vector<u64> expected{1, 2, 3};
    EXPECT_EQ(device.waits, expected);
}

TEST(FramePacer, EndFrameSubmitsTheFrameItBegan) {
    PacerDevice device;
    FakeSwapchain swapchain;
    FramePacer pacer(device);

    runFrames(pacer, swapchain, 4);

    const std::vector<u64> expected{1, 2, 3, 4};
    EXPECT_EQ(device.submits, expected);
}
