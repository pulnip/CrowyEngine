#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include "RHIDefinitions.hpp"
#include "RenderSpec.hpp"
#include "assert.hpp"
#include "enum_traits.hpp"
#include "string.hpp"
#include "LinearAllocator.hpp"
#include "Renderer.hpp"
#include "RenderPass.hpp"
#include "RenderTargetPool.hpp"
#include "Resource.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHISampler.hpp"
#include "RHIShader.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    struct PerObjectParam{
        Mat4 mvp;
    };

    template<typename T>
    using StringHashMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

    class Renderer::Impl{
    private:
        RHIDevice* device = nullptr;
        RenderTargetPool renderTargetPool;
        StringHashMap<RHISamplerPtr> samplers;
        StringHashMap<CBuffer> cbuffers;
        std::vector<RenderPass> passes;
        StringHashMap<size_t> passIndex;

        // TODO. need to fit in appropriate slot
        static constexpr uint32_t vsPerObjectCBufferSlot = 1;
        LinearBufferAllocator vsPerObjectBuffers;
        LinearBufferAllocator passParamBuffers;

    public:
        Impl(RHIDevice* device)
            : device(device)
            , vsPerObjectBuffers(*device, RHIBufferCreateDesc{
                .size = sizeof(PerObjectParam),
                .usage = combine(
                    RHIBufferUsage::ConstantBuffer,
                    RHIBufferUsage::CPUWrite
                ),
                .stride = 0,
                .initialData = nullptr
            }, "Uniform Buffer")
            , passParamBuffers(*device, RHIBufferCreateDesc{
                .size = 256,
                .usage = combine(
                    RHIBufferUsage::ConstantBuffer,
                    RHIBufferUsage::CPUWrite
                ),
                .stride = 0,
                .initialData = nullptr
            }, "PassParam Buffer"){}
        ~Impl() = default;

        void loadPasses(const RenderSpec& spec, int screenWidth, int screenHeight){
            for(const auto& [name, desc]: spec.textures){
                if(name == "BackBuffer")
                    continue;

                auto texDesc = desc;
                // fill real width
                texDesc.width  = texDesc.width  == 0 ?
                    screenWidth  : texDesc.width;
                texDesc.height = texDesc.height == 0 ?
                    screenHeight : texDesc.height;

                renderTargetPool.create(name, texDesc, *device);
            }

            for(const auto& [name, desc]: spec.samplers){
                auto newSampler = device->createSampler(desc);
                samplers.emplace(name, std::move(newSampler));
            }

            for(const auto& [name, cbuffer]: spec.cbuffers){
                cbuffers.emplace(name, cbuffer);
            }

            for(const auto& passSpec: spec.passes){
                passIndex[passSpec.name] = passes.size();
                passes.push_back(createPass(passSpec));
            }
        }

        // execute all passes
        void render(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer
        ){
            // reset Per-Frame Buffer
            vsPerObjectBuffers.reset();
            passParamBuffers.reset();

            for(const auto& pass: passes)
                executePass(pass, cmdList, ctx, backBuffer);
        }

        // execute pass with immediate compile (used for initializing Texture)
        void render(
            const RenderPassSpec& passSpec,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer
        ){
            auto pass = createPass(passSpec);

            executePass(pass, cmdList, ctx, backBuffer);
        }

        bool setPassEnabled(std::string_view passName, bool enabled){
            if(auto it = passIndex.find(passName); it != passIndex.end()){
                auto& pass = passes[it->second];

                // TODO. check pass can be disabled
                pass.enabled = enabled;
                return true;
            }
            else{
                return false;
            }
        }

        CBuffer* getCBuffer(std::string_view cbufferName){
            auto it = cbuffers.find(cbufferName);
            if(it == cbuffers.end())
                return nullptr;

            return &it->second;
        }

    private:
        RHIPipelineStatePtr createGraphicsPipelineStateHelper(
            const PipelineBindSpec& spec,
            RHIShader* vertexShader,
            RHIShader* fragmentShader,
            const std::string& name
        ){
            auto isFullscreen = spec.renderType.empty();

            RHIGraphicsPipelineStateDesc desc{
                .vertexShader = vertexShader,
                .pixelShader = fragmentShader,
                // fullscreen pipeline doesn't need vertex layout
                .vertexLayout = isFullscreen ? RHIVertexLayout{} : DEFAULT_VERTEX_LAYOUT,
                .rasterizer = spec.rasterizer,
                .depthStencil = spec.depthStencil,
                .blend = spec.blend,
                .renderTargetCount = static_cast<uint32_t>(spec.outputs.size())
            };

            for(int i=0; i<spec.outputs.size(); ++i){
                const auto& outputName = spec.outputs[i];

                if(auto tex = renderTargetPool.get(outputName))
                    desc.renderTargetFormats[i] = tex->getFormat();
                else
                    // TODO. Backbuffer
                    desc.renderTargetFormats[i] = RHITextureFormat::BGRA8_UNORM;
            }

            if(desc.depthStencil.has_value()){
                auto tex = renderTargetPool.get(spec.depthOutput);
                CROWY_ASSERT(tex != nullptr);
                CROWY_ASSERT(tex->getFormat() == desc.depthStencil->format);
            }

            return device->createGraphicsPipelineState(desc, name);
        }

        PipelineBind createPipeline(
            const PipelineBindSpec& spec,
            std::string name
        ){
            auto vs = device->createShader(RHIShaderCreateDesc{
                .file = spec.shader.vsFilePath.c_str(),
                .entry = spec.shader.vsFuncName.c_str(),
                .stage = RHIShaderStage::VertexShader
            });
            auto fs = device->createShader(RHIShaderCreateDesc{
                .file = spec.shader.fsFilePath.c_str(),
                .entry = spec.shader.fsFuncName.c_str(),
                .stage = RHIShaderStage::FragmentShader
            });
            auto pipeline = createGraphicsPipelineStateHelper(
                spec, vs.get(), fs.get(), name
            );

            std::optional<RenderTypeHash> renderType;
            if(!spec.renderType.empty())
                renderType = std::hash<RenderType>{}(spec.renderType);

            return PipelineBind{
                .name = name,
                .inputs = spec.inputs,
                .outputs = spec.outputs,
                .depthOutput = spec.depthOutput,
                .fs_samplers = spec.fs_samplers,
                .fs_cbuffers = spec.fs_cbuffers,
                .pso = std::move(pipeline),
                .renderType = renderType
            };
        }

        RenderPass createPass(
            const RenderPassSpec& spec
        ){
            std::vector<PipelineBind> pipelines;

            for(size_t i=0; i<spec.pipelines.size(); ++i){
                const auto& pipelineSpec = spec.pipelines[i];
                auto pipeline = createPipeline(
                    pipelineSpec,
                    std::format("{}[{}]", spec.name, i)
                );
                pipelines.emplace_back(std::move(pipeline));
            }

            return RenderPass{
                .name = spec.name,
                .enabled = true,
                .pipelines = std::move(pipelines)
            };
        }

        void executePass(
            const RenderPass& pass,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer
        ){
            if(pass.enabled){
                for(const auto& pipeline: pass.pipelines)
                    executePipeline(pipeline, cmdList, ctx, backBuffer);
            }
            else{
                CROWY_ASSERT(pass.pipelines.size() > 0);

                const auto& pf = pass.pipelines.front();
                const auto& pb = pass.pipelines.back();

                // bypass for Post-Process, pf.inputs[0] for bypass target
                CROWY_ASSERT(pf.inputs.size() > 0);
                auto  input = renderTargetPool.get(pf.inputs[0]);
                CROWY_ASSERT(input != nullptr);

                // not support MRT for bypass target
                CROWY_ASSERT(pb.outputs.size() == 1);
                const auto& renderTargetName = pb.outputs[0];

                if(renderTargetName != "BackBuffer"){
                    auto renderTarget = renderTargetPool.get(renderTargetName);
                    CROWY_ASSERT(renderTarget != nullptr);

                    cmdList.copy(*input, *renderTarget);
                }
                else{
                    cmdList.copy(*input, *backBuffer);
                }
            }
        }

        void executePipeline(
            const PipelineBind& pipeline,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer
        ){
            using enum RHIShaderStage;

            RHIClearColor clearColor{0.2f, 0.2f, 0.3f, 0.0f};
            RHIClearDepthStencil clearDS = {1.0f, 0};
            RHIViewport viewport{
                .x = 0.0f, .y = 0.0f,
                .width = 0.0f, .height = 0.0f,
                .minDepth = 0.0f, .maxDepth = 1.0f
            };

            CROWY_ASSERT(pipeline.outputs.size() > 0);
            const auto& renderTargetName = pipeline.outputs[0];
            auto depthTarget = renderTargetPool.get(pipeline.depthOutput);
            if(renderTargetName != "BackBuffer"){
                std::vector<RHITexture*> renderTargets(pipeline.outputs.size());

                // Multi Render Target support
                for(size_t i=0; i<pipeline.outputs.size(); ++i){
                    const auto& targetName = pipeline.outputs[i];
                    CROWY_ASSERT(targetName != "BackBuffer");
                    auto target = renderTargetPool.get(targetName);
                    CROWY_ASSERT(target != nullptr);

                    renderTargets[i] = target;
                    if(i == 0){
                        viewport.width = target->getWidth();
                        viewport.height = target->getHeight();
                    }
                    else{
                        CROWY_ASSERT(viewport.width == target->getWidth());
                        CROWY_ASSERT(viewport.height == target->getHeight());
                    }
                }

                cmdList.beginRenderPass(
                    renderTargets,
                    depthTarget,
                    // only single pass for now
                    // TODO. use RenderGraph later
                    RHILoadAction::Clear,
                    RHIStoreAction::Store,
                    clearColor,
                    clearDS,
                    pipeline.name.c_str()
                );
            }
            else{
                viewport.width = backBuffer->getWidth();
                viewport.height = backBuffer->getHeight();

                cmdList.beginRenderPass(
                    *backBuffer,
                    depthTarget,
                    RHILoadAction::Load,
                    RHIStoreAction::Store,
                    clearColor,
                    clearDS,
                    pipeline.name.c_str()
                );
            }
            cmdList.setPipelineState(pipeline.pso.get());

            // bind input texture
            for(size_t i=0; i<pipeline.inputs.size(); ++i){
                auto shaderResource = renderTargetPool.get(pipeline.inputs[i]);
                if(shaderResource == nullptr)
                    continue;

                // transition input texture to shader resource state
                cmdList.transitionBarrier(
                    *shaderResource,
                    RHIResourceState::AllShaderResource
                );

                // TODO. select shader stage for advanced rendering technique
                cmdList.setTexture(
                    static_cast<uint32_t>(i),
                    *shaderResource,
                    FragmentShader
                );
            }

            for(const auto& samplerBind: pipeline.fs_samplers){
                auto it = samplers.find(samplerBind.name);
                [[unlikely]] if(it == samplers.end())
                    continue;

                cmdList.setSampler(
                    samplerBind.slot, *it->second,
                    FragmentShader
                );
            }

            for(const auto& cbufferBind: pipeline.fs_cbuffers){
                auto it = cbuffers.find(cbufferBind.name);
                [[unlikely]] if(it == cbuffers.end())
                    continue;

                const auto& cbuf = it->second;
                auto& buf = passParamBuffers.acquire();
                buf.upload(cbuf.buffer.data(), cbuf.buffer.size());

                cmdList.setConstantBuffer(
                    FragmentShader,
                    cbufferBind.slot, buf
                );
            }

            cmdList.setViewport(viewport);
            cmdList.setScissorRect(RHIScissorRect{
                .left   = static_cast<int32_t>(viewport.x),
                .top    = static_cast<int32_t>(viewport.y),
                .right  = static_cast<int32_t>(viewport.width),
                .bottom = static_cast<int32_t>(viewport.height)
            });

            // draw
            if(pipeline.isFullscreenPass()){
                drawFullscreenQuad(cmdList, ctx);
            }
            else{
                drawObjectsWithType(cmdList, ctx, *pipeline.renderType);
            }

            cmdList.endRenderPass();
        }

        void drawObjectsWithType(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RenderTypeHash& type
        ){
            using enum RHIShaderStage;

            if(ctx.renderItems.empty())
                return;

            for(const auto& renderItem: ctx.renderItems){
                if(renderItem.type != type)
                    continue;

                auto mesh = get(renderItem.mesh);
                auto materialSet = get(renderItem.materials);

                PerObjectParam perObjectParam{
                    .mvp = ctx.proj * ctx.view * renderItem.world
                };
                auto& buf = vsPerObjectBuffers.acquire();

                buf.upload(&perObjectParam, sizeof(PerObjectParam));

                cmdList.setConstantBuffer(VertexShader, vsPerObjectCBufferSlot, buf);

                for(const auto& submesh: mesh){
                    // TODO. hide slot number (bc it's for Metal)
                    cmdList.setVertexBuffer(0, *submesh.vertexBuffer.get(), sizeof(Vertex), 0);
                    cmdList.setIndexBuffer(*submesh.indexBuffer.get(),
                        RHIIndexFormat::UInt32, 0);

                    auto it = materialSet.find(submesh.materialSlotName);
                    if(it == materialSet.end())
                        continue;
                    cmdList.setTexture(0, *it->second->baseColorMap.get(), FragmentShader);

                    cmdList.drawIndexed(submesh.indexCount, 1);
                }
            }
        }

        void drawFullscreenQuad(
            RHICommandList& cmdList,
            const RenderContext& ctx
        ){
            cmdList.draw(6, 1);
        }
    };

    Renderer::Renderer(RHIDevice* device)
        :impl(std::make_unique<Impl>(device))
    {}

    Renderer::~Renderer() = default;

    void Renderer::loadPasses(const RenderSpec& spec, int screenWidth, int screenHeight){
        impl->loadPasses(spec, screenWidth, screenHeight);
    }

    bool Renderer::setPassEnabled(std::string_view passName, bool enabled){
        return impl->setPassEnabled(passName, enabled);
    }

    void Renderer::render(
        RHICommandList& cmdList,
        const RenderContext& ctx,
        RHISwapchain* backBuffer
    ){
        impl->render(cmdList, ctx, backBuffer);
    }

    void Renderer::render(
        const RenderPassSpec& passSpec,
        RHICommandList& cmdList,
        const RenderContext& ctx,
        RHISwapchain* backBuffer
    ){
        impl->render(passSpec, cmdList, ctx, backBuffer);
    }

    CBuffer* Renderer::getCBuffer(std::string_view cbufferName){
        return impl->getCBuffer(cbufferName);
    }
}
