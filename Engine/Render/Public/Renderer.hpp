#pragma once

#include <memory>
#include <span>
#include "math.hpp"
#include "RenderDefinitions.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
    struct RenderSpec;

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

        void loadPasses(const RenderSpec&);
        // execute all passes
        void render(
            RHICommandList&,
            const RenderContext&
        );
        // TODO. execute specific render pass
        // void render(
        //     RHICommandList&, 
        //     const RenderContext&,
        //     const std::string& passName
        // );
    };
}