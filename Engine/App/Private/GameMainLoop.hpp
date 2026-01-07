#pragma once

#include "EntityRegistry.hpp"
#include "MainLoop.hpp"
#include "SystemScheduler.hpp"
#include "UpdateContext.hpp"

namespace Crowy
{
    using ECSScheduler = SystemScheduler<UpdateContext>;

    class GameMainLoop: public MainLoop{
    private:
        EntityRegistry registry;
        ECSScheduler scheduler;

    public:
        void initialize() override;
        bool update(float dt) override;
        void finalize() override;
    };
}