#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include "Log.hpp"
#include "Renderer.hpp"
#include "RenderPass.hpp"
#include "RenderSpec.hpp"
#include "RenderTargetPool.hpp"
#include "Resource.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    static RHIPipelineStatePtr createGraphicsPipelineStateHelper(
        RHIDevice& device,
        const RenderPassSpec& spec,
        RHIShader* vertexShader,
        RHIShader* fragmentShader,
        const std::unordered_map<std::string, RenderTargetSpec>& renderTargets
    ){
        RHIGraphicsPipelineStateDesc desc{
            .vertexShader = vertexShader,
            .pixelShader = fragmentShader,
            // use default vertex layout and topology
            .rasterizer = spec.rasterizer,
            .depthStencil = spec.depthStencil,
            .blend = spec.blend,
            .renderTargetCount = static_cast<uint32_t>(spec.targets.size()),
            .debugName = spec.name.c_str()
        };

        for(int i=0; i<spec.targets.size(); ++i){
            const auto& targetName = spec.targets[i];
            if(auto it=renderTargets.find(targetName); it!=renderTargets.end()){
                desc.renderTargetFormats[i] = it->second.desc.format;
            }
        }
        if(auto it=renderTargets.find(spec.depthTarget); it!=renderTargets.end()){
            desc.depthStencilFormat = it->second.desc.format;
        }

        return device.createGraphicsPipelineState(desc);
    }

    class Renderer::Impl{
    private:
        RHIDevice* device = nullptr;
        std::vector<RenderPass> passes;
        std::unordered_map<std::string, size_t> passIndex;
        RenderTargetPool renderTargetPool;

    public:
        Impl(RHIDevice* device)
            :device(device)
        {}
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

                auto pipeline = createGraphicsPipelineStateHelper(
                    *device, passSpec, vs.get(), fs.get(),
                    spec.renderTargets
                );

                std::optional<RenderTypeHash> renderType = std::nullopt;
                if(!passSpec.renderType.empty())
                    renderType = std::hash<RenderType>{}(passSpec.renderType);

                const auto index = passes.size();
                passIndex[passSpec.name] = index;
                passes.push_back(RenderPass{
                    .name = passSpec.name,
                    .renderType = renderType,
                    .vs = std::move(vs), .fs = std::move(fs),
                    .pipeline = std::move(pipeline),
                    .inputs = passSpec.inputs,
                    .targets = passSpec.targets,
                    .depthTarget = passSpec.depthTarget
                });
            }

            for(const auto& [name, renderTarget]: spec.renderTargets){
                if(name != "BackBuffer")
                    renderTargetPool.create(name, renderTarget.desc, *device);
            }
        }

        // execute all passes
        void render(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer
        ){
            for(const auto& pass: passes){
                executePass(cmdList, ctx, backBuffer, pass);
            }
        }
        // execute specific render pass
        void render(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer,
            const std::string& name
        ){
            if(auto it = passIndex.find(name); it != passIndex.end()){
                auto& pass = passes[it->second];

                executePass(cmdList, ctx, backBuffer, pass);
            }
        }

    private:
        void executePass(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer,
            const RenderPass& pass
        ){
            RHIClearColor clearColor{0.2f, 0.2f, 0.3f, 1.0f};

            cmdList.setPipelineState(pass.pipeline.get());
            // set RenderTarget
            // TODO. multi render target
            const auto& renderTargetName = pass.targets[0];
            auto depthTarget = renderTargetPool.get(pass.depthTarget);
            if(renderTargetName != "BackBuffer"){
                auto renderTarget = renderTargetPool.get(renderTargetName);
                cmdList.beginRenderPass(
                    renderTarget,
                    depthTarget,
                    // only single pass for now
                    // TODO. use RenderGraph later
                    RHILoadStoreAction::Clear,
                    RHILoadStoreAction::Store,
                    clearColor
                );
            }
            else{
                cmdList.beginRenderPass(
                    backBuffer,
                    depthTarget,
                    RHILoadStoreAction::Clear,
                    RHILoadStoreAction::Store,
                    clearColor
                );
            }

            cmdList.setViewport(ctx.viewport);
            cmdList.setScissorRect(RHIScissorRect{
                .left   = static_cast<int32_t>(ctx.viewport.x     ),
                .top    = static_cast<int32_t>(ctx.viewport.y     ),
                .right  = static_cast<int32_t>(ctx.viewport.width ),
                .bottom = static_cast<int32_t>(ctx.viewport.height)
            });

            cmdList.setPipelineState(pass.pipeline.get());

            // draw
            if(pass.isFullscreenPass()){
                drawFullscreenQuad(cmdList);
            }
            else{
                drawObjectsWithType(cmdList, ctx, *pass.renderType);
            }
        }

        void drawObjectsWithType(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RenderTypeHash& type
        ){
            if(ctx.renderItems.empty())
                return;

            for(const auto& renderItem: ctx.renderItems){
                if(renderItem.type != type)
                    continue;

                auto mesh = get(renderItem.mesh);
                auto materialSet = get(renderItem.materials);
            }
        }

        void drawFullscreenQuad(RHICommandList& cmdList){
            // TODO. fullscreen triangle (created from vertex shader)
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
        const RenderContext& ctx,
        RHISwapchain* backBuffer
    ){
        impl->render(cmdList, ctx, backBuffer);
    }
}
