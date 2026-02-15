#pragma once

#include "System.hpp"
#include "UIRenderer.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct UpdateContext;
    class Renderer;
    class UIRenderer;

    class RenderSystem: public System<UpdateContext>{
    private:
        Renderer& renderer;
        Widget& ui;
        UIRenderer& uiRenderer;

    public:
        RenderSystem(
            Renderer& renderer,
            Widget& ui, UIRenderer& uiRenderer
        )
            : renderer(renderer)
            , ui(ui), uiRenderer(uiRenderer)
        {}

        void update(EntityRegistry&, UpdateContext&) override;
    };
}