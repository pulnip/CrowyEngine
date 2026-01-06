#pragma once

#include "EntityRegistry.hpp"
#include "MainLoop.hpp"

namespace Crowy
{
    class GameMainLoop: public MainLoop{
    private:
        EntityRegistry registry;

    public:
        void initialize() override;
        bool update(float dt) override;
        void finalize() override;
    };
}