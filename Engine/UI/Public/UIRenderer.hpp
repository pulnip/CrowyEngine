#pragma once

#include <functional>
#include <memory>
#include <span>
#include <string_view>
#include "RHIFWD.hpp"
#include "Widget.hpp"

namespace Crowy
{
    class UIRenderer{
        class Impl;
        std::unique_ptr<Impl> impl;

    public:
        UIRenderer(void* window, RHIDevice&, RHICommandList&);
        ~UIRenderer();

        void render(
            std::string_view uiName,
            Widget& ui,
            CROWY_UI_CONTEXT&,
            RHICommandList&,
            RHISwapchain*
        );
    };
}