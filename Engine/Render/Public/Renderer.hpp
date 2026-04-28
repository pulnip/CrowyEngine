#pragma once

#include <memory>
#include "RenderSpec.hpp"
#include "RHIFWD.hpp"
#include "RenderContext.hpp"

namespace Crowy
{
    struct RenderSpec;
    struct CBuffer;

    class Renderer{
        class Impl;
        std::unique_ptr<Impl> impl;

    public:
        Renderer(RHIDevice& device);
        ~Renderer();

        void loadPasses(const RenderSpec&, int screenWidth = 0, int screenHeight = 0);
        // execute all passes
        void render(
            RHICommandList&,
            const RenderContext& = {},
            RHISwapchain* = nullptr
        );

        // execute pass with immediate compile (used for initializing Texture)
        void render(
            const RenderPassSpec& passSpec,
            RHICommandList& cmdList,
            const RenderContext& ctx = {},
            RHISwapchain* backBuffer = nullptr
        );

        bool setPassEnabled(std::string_view passName, bool enabled);
        CBuffer* getCBuffer(std::string_view cbufferName);
        RHIBuffer* getBuffer(std::string_view bufferName);
    };
}