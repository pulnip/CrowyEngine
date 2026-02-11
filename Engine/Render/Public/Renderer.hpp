#pragma once

#include <memory>
#include <span>
#include "math.hpp"
#include "CBuffer.hpp"
#include "RenderDefinitions.hpp"
#include "ResourceHandle.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct RenderSpec;

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
        RHIViewport viewport;
    };

    class Renderer{
        class Impl;
        std::unique_ptr<Impl> impl;

    public:
        Renderer(RHIDevice* device);
        ~Renderer();

        void loadPasses(const RenderSpec&, int screenWidth, int screenHeight);
        // execute all passes
        void render(
            RHICommandList&,
            const RenderContext&,
            RHISwapchain*
        );
        // TODO. execute specific render pass
        // void render(
        //     RHICommandList&, 
        //     const RenderContext&,
        //     RHISwapchain*
        //     const std::string& passName
        // );

        bool setPassEnabled(std::string_view passName, bool enabled);
        CBuffer* getCBuffer(std::string_view cbufferName);
    };
}