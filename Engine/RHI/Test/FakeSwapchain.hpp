#pragma once

#include <exception>

#include "RHISwapchain.hpp"

namespace Crowy
{
    // presents nothing: the tests drive the swapchain's own bookkeeping,
    // and the frame dump's backend half is theirs to call
    class FakeSwapchain final : public RHISwapchain {
    public:
        using RHISwapchain::TakeFrameDump;

        FakeSwapchain()
            : RHISwapchain(RHIPixelFormat::RGBA8_UNORM) {}

        bool AcquireNextImage() override { return true; }
        void Resize(u32, u32) override {}
        u32 GetWidth() const noexcept override { return 0; }
        u32 GetHeight() const noexcept override { return 0; }
        RHITexture& GetCurrentTexture() override { std::terminate(); }
    };
}
