#pragma once

#include "System.hpp"
#include "UIRenderer.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct UpdateContext;

    class PlayerSystem: public System<UpdateContext>{
    public:
        void update(EntityRegistry&, UpdateContext&) override;
    };

    class ScriptSystem: public System<UpdateContext>{
    public:
        void start(EntityRegistry&) override;
        void update(EntityRegistry&, UpdateContext&) override;
        void finish(EntityRegistry&) override;
    };

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