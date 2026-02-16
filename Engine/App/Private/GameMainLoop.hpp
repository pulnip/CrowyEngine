#pragma once

#include "AppUI.hpp"
#include "EntityRegistry.hpp"
#include "MainLoop.hpp"
#include "Renderer.hpp"
#include "SystemScheduler.hpp"
#include "Context.hpp"
#define CROWY_UI_CONTEXT UIContext
#include "UIRenderer.hpp"

namespace Crowy
{
    struct InputSpec;
    struct ScriptSpec;
    struct SceneSpec;
    using ECSScheduler = SystemScheduler<UpdateContext>;

    class GameMainLoop: public MainLoop{
    private:
        EntityRegistry registry;
        ECSScheduler scheduler;

        Renderer renderer;
        Widget ui = cbufferInspector("FocusParams");
        UIRenderer uiRenderer;

    public:
        GameMainLoop(const RenderSpec&,
            const InputSpec&, const ScriptSpec&,
            const SceneSpec&
        );

        void initialize() override;
        bool update(float deltaTime, float totalTime) override;
        void finalize() override;
    };
}