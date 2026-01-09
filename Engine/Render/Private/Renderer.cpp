#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include "Log.hpp"
#include "Renderer.hpp"
#include "RenderPass.hpp"
#include "RenderSpec.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    class Renderer::Impl{
    private:
        RHIDevice* device = nullptr;
        std::vector<RenderPass> passes;
        std::unordered_map<std::string, size_t> passIndex;

    public:
        Impl(RHIDevice* device)
            :device(device){}
        ~Impl() = default;

        void loadPasses(const RenderSpec& spec){
            for(const auto& passSpec: spec.passes){
                auto vs = device->createShader(RHIShaderCreateDesc{
                    .file = passSpec.shader.vsFilePath.c_str(),
                    .entry = passSpec.shader.vsFuncName.c_str(),
                    .stage = RHIShaderStage::VertexShader,
                    .debugName = passSpec.shader.vsFilePath.c_str()
                });
                auto fs = device->createShader(RHIShaderCreateDesc{
                    .file = passSpec.shader.fsFilePath.c_str(),
                    .entry = passSpec.shader.fsFuncName.c_str(),
                    .stage = RHIShaderStage::FragmentShader,
                    .debugName = passSpec.shader.fsFilePath.c_str()
                });
                auto pipeline = device->createGraphicsPipelineState(RHIGraphicsPipelineStateDesc{
                    .vertexShader = vs.get(),
                    .pixelShader = fs.get(),
                    .debugName = passSpec.name.c_str()
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

                const auto index = passes.size();
                passIndex[passSpec.name] = index;
                passes.push_back(RenderPass{
                    .name = passSpec.name,
                    // .renderType =
                    .vs = std::move(vs), .fs = std::move(fs),
                    .pipeline = std::move(pipeline),
                    .bindings = bindings,
                });
            }
        }

        // execute all passes
        void render(
            RHICommandList& cmdList,
            const RenderContext& ctx
        ){
            for(const auto& pass: passes){
                executePass(cmdList, ctx, pass);
            }
        }
        // execute specific render pass
        void render(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const std::string& name
        ){
            if(auto it = passIndex.find(name); it != passIndex.end()){
                auto& pass = passes[it->second];
                executePass(cmdList, ctx, pass);
            }
        }

    private:
        void executePass(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RenderPass& pass
        ){
            cmdList.setPipelineState(pass.pipeline.get());

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

        void drawObjectsWithType(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RenderType& type
        ){
            if(ctx.renderItems.empty())
                return;

            
        }

        void drawFullscreenQuad(RHICommandList& cmdList){
            // fullscreen triangle (created from vertex shader)
            // cmdList.draw(3);
        }
    };

    Renderer::Renderer(RHIDevice* device)
        :impl(std::make_unique<Impl>(device))
    {}

    Renderer::~Renderer() = default;

    void Renderer::loadPasses(const RenderSpec& spec){
        impl->loadPasses(spec);
    }

    void Renderer::render(
        RHICommandList& cmdList,
        const RenderContext& ctx
    ){
        impl->render(cmdList, ctx);
    }
}
