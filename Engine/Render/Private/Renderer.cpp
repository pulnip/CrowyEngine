#include "EntityRegistry.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"
#include "Resource.hpp"
#include "RHICommandList.hpp"

namespace Crowy
{
    Renderer::Renderer(RHIDevice* device)
        :device(device){}

    Renderer::~Renderer() = default;

    void Renderer::loadPasses(const RenderSpec& spec){
        for(const auto& passSpec: spec.passes){
            auto shader = getOrLoad(ShaderRequest{
                .vsFilePath = passSpec.shader.vsFilePath,
                .vsFuncName = passSpec.shader.vsFuncName,
                .fsFilePath = passSpec.shader.fsFilePath,
                .fsFuncName = passSpec.shader.fsFuncName
            });

            // targets -> ResourceBinding (Write)
            std::vector<ResourceBinding> bindings;
            bindings.reserve(passSpec.targets.size());

            for(const auto& target: passSpec.targets){
                bindings.push_back({
                    .name = target,
                    .usage = ResourceUsage::Write
                });
            }

            // TODO: parse renderType from passSpec

            registerPass(RenderPass{
                .name = passSpec.name,
                .shader = shader,
                // .target = 
                .bindings = bindings,
            });
        }
    }

    void Renderer::registerPass(RenderPass pass){
        const auto name = pass.name;
        const auto index = passes.size();

        passes.push_back(std::move(pass));
        passIndex[name] = index;
    }

    void Renderer::render(
        RHICommandList& cmdList,
        const RenderContext& ctx
    ){
        for(const auto& pass: passes){
            executePassInternal(cmdList, ctx, pass);
        }
    }

    void Renderer::executePass(
        RHICommandList& cmdList,
        const RenderContext& ctx,
        const std::string& passName
    ){
        if(auto pass = getPass(passName)){
            executePassInternal(cmdList, ctx, *pass);
        }
    }

    void Renderer::reorderPasses(const std::vector<std::string>& order){
        std::vector<RenderPass> reordered;
        reordered.reserve(order.size());

        for(const auto& name: order){
            if(auto* pass = getPass(name)){
                reordered.push_back(std::move(*pass));
            }
        }

        passes = std::move(reordered);

        passIndex.clear();
        for(size_t i = 0; i < passes.size(); ++i){
            passIndex[passes[i].name] = i;
        }
    }

    void Renderer::executePassInternal(
        RHICommandList& cmdList,
        const RenderContext& ctx,
        const RenderPass& pass
    ){
        // bind shader
        auto shader = get(pass.shader);
        if(!shader || shader->isValid()){
            LOG_ERROR(LOG_RENDER, "RenderPass has invalid shader");
            return;
        }
        // cmdList.setShader(shader->vertexShader, shader->fragmentShader);

        // set RenderTarget
        for(const auto& binding: pass.bindings){
            if(binding.usage == ResourceUsage::Write){
                // auto texture = getOrCreateTexture(binding.name);
                // cmdList.setRenderTarget(texture);
            }
        }

        // draw
        if(pass.isFullscreenPass()){
            drawFullscreenQuad(cmdList);
        }
        else{
            drawObjectsWithType(cmdList, ctx, pass.renderType);
        }
    }

    void Renderer::drawObjectsWithType(
        RHICommandList& cmdList,
        const RenderContext& ctx,
        const RenderType& type
    ){
        if(!ctx.queue.empty()) return;
    }

    void Renderer::drawFullscreenQuad(RHICommandList& cmdList){
        // fullscreen triangle (created from vertex shader)
        // cmdList.draw(3);
    }
}
