#pragma once

#include "EntityRegistry.hpp"
#include "MainLoop.hpp"
#include "Renderer.hpp"
#include "RHIFWD.hpp"
#include "SystemScheduler.hpp"
#include "Context.hpp"
#define CROWY_UI_CONTEXT UIContext
#include "UIRenderer.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct SceneSpec;
    using ECSScheduler = SystemScheduler<UpdateContext>;

    class GameMainLoop: public MainLoop{
    private:
        EntityRegistry registry;
        ECSScheduler scheduler;

        Renderer renderer;
        Widget ui = demoUI();
        UIRenderer uiRenderer;

    public:
        GameMainLoop(const SceneSpec&, const RenderSpec&);

        void initialize() override;
        bool update(float deltaTime, float totalTime) override;
        void finalize() override;
    };
}