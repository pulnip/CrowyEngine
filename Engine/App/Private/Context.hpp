#pragma once

namespace Crowy
{
    struct UpdateContext{
        const float deltaTime;
        const float totalTime;
    };

    class Renderer;

    struct UIContext{
        Renderer& renderer;
    };
}