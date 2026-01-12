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
#include "RHIBuffer.hpp"
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
        const std::unordered_map<std::string, RHITextureCreateDesc>& renderTargets
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
                desc.renderTargetFormats[i] = it->second.format;
            }
        }
        if(auto it=renderTargets.find(spec.depthTarget); it!=renderTargets.end()){
            desc.depthStencilFormat = it->second.format;
        }

        return device.createGraphicsPipelineState(desc);
    }

    class Renderer::Impl{
    private:
        RHIDevice* device = nullptr;
        std::vector<RenderPass> passes;
        std::unordered_map<std::string, size_t> passIndex;
        RenderTargetPool renderTargetPool;

        RHIBufferPtr uniformBuffer;

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
                    .debugName = nullptr
                });
                auto fs = device->createShader(RHIShaderCreateDesc{
                    .file = passSpec.shader.fsFilePath.c_str(),
                    .entry = passSpec.shader.fsFuncName.c_str(),
                    .stage = RHIShaderStage::FragmentShader,
                    .debugName = nullptr
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

            uniformBuffer = device->createBuffer({
                // TODO. use Unified Constant buffer + offset later.
                .size = sizeof(Mat4),
                .usage = RHIBufferUsage::ConstantBuffer,
                .stride = 0,
                .initialData = nullptr,
                .debugName = "MVP Uniform Buffer"
            });

            for(const auto& [name, renderTarget]: spec.renderTargets){
                if(name != "BackBuffer")
                    renderTargetPool.create(name, renderTarget, *device);
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
            cmdList.setPipelineState(pass.pipeline.get());

            cmdList.setViewport(ctx.viewport);
            cmdList.setScissorRect(RHIScissorRect{
                .left   = static_cast<int32_t>(ctx.viewport.x     ),
                .top    = static_cast<int32_t>(ctx.viewport.y     ),
                .right  = static_cast<int32_t>(ctx.viewport.width ),
                .bottom = static_cast<int32_t>(ctx.viewport.height)
            });

            // draw
            if(pass.isFullscreenPass()){
                drawFullscreenQuad(cmdList);
            }
            else{
                drawObjectsWithType(cmdList, ctx, *pass.renderType);
            }

            cmdList.endRenderPass();
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

                auto mvp = transpose(ctx.proj * ctx.view * renderItem.world);
                // TODO. use offset later
                uniformBuffer->update(mvp.data(), sizeof(Mat4));

                // TODO. need to fit in appropriate slot
                cmdList.setConstantBuffer(RHIShaderStage::VertexShader, 1, uniformBuffer.get());

                for(const auto& submesh: mesh){
                    // TODO. hide slot number (bc it's for Metal)
                    cmdList.setVertexBuffer(0, submesh.vertexBuffer.get(), sizeof(Crowy::Vertex), 0);
                    cmdList.setIndexBuffer(submesh.indexBuffer.get(),
                        RHIIndexFormat::UInt32, 0);

                    auto it = materialSet.find(submesh.materialSlotName);
                    if(it == materialSet.end())
                        continue;
                    cmdList.setTexture(0, it->second->baseColorMap.get(),
                        RHIShaderStage::FragmentShader);

                    cmdList.drawIndexed(submesh.indexCount, 1);
                }
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
