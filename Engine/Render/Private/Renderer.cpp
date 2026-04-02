#include <cstddef>
#include "RenderSpec.hpp"
#include "Renderer.hpp"
#include "RenderGraph.hpp"

namespace Crowy
{
    class Renderer::Impl{
    private:
        RenderGraph graph;

    public:
        Impl(RHIDevice* device)
            : graph(device) {}
        ~Impl() = default;

        void loadPasses(
            const RenderSpec& spec,
            int screenWidth,
            int screenHeight
        ){
            graph.loadPasses(
                spec,
                screenWidth,
                screenHeight
            );
        }

        // execute all render passes
        void render(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* swapchain
        ){
            graph.compile();

            graph.execute(
                cmdList,
                ctx,
                swapchain
            );
        }

        void render(
            const RenderPassSpec& passSpec,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* swapchain
        ){
            graph.instantExecute(
                passSpec,
                cmdList,
                ctx,
                swapchain
            );
        }

        CBuffer* getCBuffer(std::string_view cbufferName){
            return graph.getCBuffer(cbufferName);
        }

        RHIBuffer* getBuffer(std::string_view bufferName){
            return graph.getBuffer(bufferName);
        }

        bool setPassEnabled(std::string_view passName, bool enabled){
            return graph.setPassEnabled(passName, enabled);
        }
    };

    Renderer::Renderer(RHIDevice* device)
        :impl(std::make_unique<Impl>(device))
    {}

    Renderer::~Renderer() = default;

    void Renderer::loadPasses(
        const RenderSpec& spec,
        int screenWidth,
        int screenHeight
    ){
        impl->loadPasses(
            spec,
            screenWidth,
            screenHeight
        );
    }

    bool Renderer::setPassEnabled(std::string_view passName, bool enabled){
        return impl->setPassEnabled(passName, enabled);
    }

    void Renderer::render(
        RHICommandList& cmdList,
        const RenderContext& ctx,
        RHISwapchain* swapchain
    ){
        impl->render(
            cmdList,
            ctx,
            swapchain
        );
    }

    void Renderer::render(
        const RenderPassSpec& passSpec,
        RHICommandList& cmdList,
        const RenderContext& ctx,
        RHISwapchain* swapchain
    ){
        impl->render(
            passSpec,
            cmdList,
            ctx,
            swapchain
        );
    }

    CBuffer* Renderer::getCBuffer(std::string_view cbufferName){
        return impl->getCBuffer(cbufferName);
    }

    RHIBuffer* Renderer::getBuffer(std::string_view bufferName){
        return impl->getBuffer(bufferName);
    }
}
