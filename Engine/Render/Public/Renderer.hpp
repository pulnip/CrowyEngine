#pragma once

#include <memory>
#include <span>
#include "math.hpp"
#include "RenderDefinitions.hpp"
#include "RenderSpec.hpp"
#include "ResourceHandle.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct RenderSpec;
    struct CBuffer;

    struct RenderItem{
        MeshHandle mesh;
        MaterialSetHandle materials;
        Mat4 world;
        RenderTypeHash type;
    };

    struct RenderContext{
        std::span<const RenderItem> renderItems;
        // Camera Information
        Mat4 view, proj;
    };

    class Renderer{
        class Impl;
        std::unique_ptr<Impl> impl;

    public:
        Renderer(RHIDevice* device);
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