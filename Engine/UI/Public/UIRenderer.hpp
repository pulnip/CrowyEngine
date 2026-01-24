#pragma once

#include <functional>
#include <memory>
#include <span>
#include "RHIFWD.hpp"

namespace Crowy
{
    class UIRenderer{
        class Impl;
        std::unique_ptr<Impl> impl;

    public:
        UIRenderer(void* window, RHIDevice&, RHICommandList&);
        ~UIRenderer();

        void render(
            RHICommandList&,
            std::function<void(void)> ctx,
            RHISwapchain*
        );
    };
}