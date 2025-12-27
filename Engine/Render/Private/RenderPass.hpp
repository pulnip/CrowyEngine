#pragma once

namespace Crowy
{
    class RenderGraphBuilder;
    struct RenderContext;

    class RenderPass{
    public:
        virtual ~RenderPass() = default;

        virtual void setup(RenderGraphBuilder& builder) = 0;

        virtual void execute(RenderContext& ctx) = 0;
    };
}