#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "RenderPass.hpp"

namespace Crowy
{
    struct RenderSpec;
    class RHIDevice;
    class RHICommandList;

    class Renderer{
    private:
        RHIDevice* device = nullptr;
        std::vector<RenderPass> passes;
        std::unordered_map<std::string, size_t> passIndex;

    public:
        Renderer(RHIDevice* device);
        ~Renderer();

        void loadPasses(const RenderSpec&);
        // for coded render pass
        void registerPass(RenderPass);
        // execute all passes
        void render(RHICommandList&, const RenderContext&);
        // execute specific render pass
        void executePass(
            RHICommandList&, 
            const RenderContext&,
            const std::string& passName
        );

        inline RenderPass* getPass(const std::string& name){
            return const_cast<RenderPass*>(
                static_cast<const Renderer*>(this)->getPass(name)
            );
        }
        const RenderPass* getPass(const std::string& name) const{
            auto it = passIndex.find(name);
            if(it != passIndex.end()){
                return &passes[it->second];
            }
            return nullptr;
        }

        void reorderPasses(const std::vector<std::string>& order);

    private:
        void executePassInternal(
            RHICommandList&,
            const RenderContext&,
            const RenderPass&
        );

        void drawObjectsWithType(
            RHICommandList&,
            const RenderContext&,
            const RenderType&
        );

        void drawFullscreenQuad(RHICommandList&);
    };
}