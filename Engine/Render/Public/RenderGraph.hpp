#pragma once

#include <cstddef>
#include <vector>
#include "RenderPass.hpp"
#include "ResourceHandle.hpp"

namespace Crowy
{
    class RenderGraph{
    private:
        std::vector<std::vector<size_t>> depsAdjList;
        std::vector<size_t> executionOrder;

        struct ResourceLifetime{
            size_t firstUse;
            size_t lastUse;
        };
        std::vector<ResourceLifetime> lifetimes;

    public:
        template<typename SetupFunc, typename ExecFunc>
        RenderPass& addPass(std::string name,
            SetupFunc setupFunc,
            ExecFunc execFunc
        ){

        }

        // createRenderTarget
        // importTexture?

        void compile();

        void execute(RHICommandList&);
    };
}