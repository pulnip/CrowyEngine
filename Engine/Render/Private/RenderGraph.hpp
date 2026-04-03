#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>
#include "ComputePass.hpp"
#include "LinearAllocator.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RenderPass.hpp"
#include "Resource.hpp"
#include "RenderContext.hpp"
#include "RenderSpec.hpp"
#include "ResourceHandle.hpp"
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

    struct RenderGraphCompileOptions{
        // TODO. not used for now.
        bool strictMemoryAliasing = false;
    };

    enum class AccessMode{
        Read,
        Write,
        ReadWrite,
        CopySrc,
        CopyDst
    };

    struct AbsoluteSize{
        uint32_t width = 0;
        uint32_t height = 0;
    };

    struct RelativeSize{
        float scale = 1.0f;
    };

    struct Inherited{};

    using SizePolicy = std::variant<
        AbsoluteSize,
        RelativeSize,
        Inherited
    >;

    struct ResourceDesc{
        SizePolicy size;
        // Unknown for Buffer, else for Texture
        RHITextureFormat format = RHITextureFormat::Unknown;
    };

    struct PipelineDesc{

    };

    class RenderGraph{
    private:
        struct ResourceLifetime{
            size_t firstUse;
            size_t lastUse;
        };

    private:
        RHIDevice* device = nullptr;

        // TODO. use pool for resource reuse and memory aliasing later.
        StringHashMap<RHIBufferPtr> buffers;
        StringHashMap<RHITexturePtr> textures;
        StringHashMap<RHISamplerPtr> samplers;
        StringHashMap<CBuffer> cbuffers;

        // ordered by execution order
        std::vector<RenderPass> renderPasses;
        StringHashMap<size_t> renderPassIndex;
        std::vector<ComputePass> computePasses;
        StringHashMap<size_t> computePassIndex;

        enum class PassType{
            ComputePass,
            RenderPass
        };
        struct PassIndex{
            PassType type;
            size_t index;
        };
        std::vector<PassIndex> executionOrder;

        // TODO. need to fit in appropriate slot
        static constexpr uint32_t vsPerObjectCBufferSlot = 1;
        LinearBufferAllocator vsPerObjectBuffers;
        LinearBufferAllocator passParamBuffers;

    public:
        RenderGraph(RHIDevice* device)
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
        ~RenderGraph() = default;

        void loadPasses(
            const RenderSpec& spec,
            int screenWidth = 0,
            int screenHeight = 0
        ){
            loadBuffers(spec.buffers);
            loadSamplers(spec.samplers);
            loadTextures(spec.textures,
                screenWidth,
                screenHeight
            );
            loadCBuffers(spec.cbuffers);

            loadRenderPasses(spec.renderPasses);
            loadComputePasses(spec.computePasses);
        }

    private:
        void loadBuffers(
            const std::unordered_map<
                std::string,
                RHIBufferCreateDesc
            >& descs
        ){
            for(const auto& [
                name,
                desc
            ]: descs){
                buffers.emplace(
                    name,
                    device->createBuffer(desc)
                );
            }
        }

        void loadTextures(
            const std::unordered_map<
                std::string,
                RHITextureCreateDesc
            >& descs,
            int screenWidth,
            int screenHeight
        ){
            for(const auto& [
                name,
                desc
            ]: descs){
                if(name == "BackBuffer")
                    continue;

                auto resolved = desc;
                resolved.width = resolved.width == 0 ?
                    screenWidth : resolved.width;
                resolved.height = resolved.height == 0 ?
                    screenHeight : resolved.height;

                textures.emplace(
                    name,
                    device->createTexture(resolved)
                );
            }
        }

        void loadSamplers(
            const std::unordered_map<
                std::string,
                RHISamplerState
            >& descs
        ){
            for(const auto& [
                name,
                desc
            ]: descs){
                samplers.emplace(
                    name,
                    device->createSampler(desc)
                );
            }
        }

        void loadCBuffers(
            const std::unordered_map<
                std::string,
                CBuffer
            >& cbufs
        ){
            for(const auto& [
                name,
                cbuf
            ]: cbufs){
                cbuffers.emplace(
                    name,
                    cbuf
                );
            }
        }

        void loadRenderPasses(
            std::span<const RenderPassSpec> specs
        ){
            for(const auto& spec: specs){
                renderPassIndex[spec.name] = renderPasses.size();
                renderPasses.push_back(createPass(spec));
            }
        }

        RenderPass createPass(
            const RenderPassSpec& spec
        ){
            std::vector<GraphicsPipelineBind> pipelines;

            for(size_t i=0; i<spec.pipelines.size(); ++i){
                const auto& pipelineSpec = spec.pipelines[i];
                auto pipeline = createPipeline(
                    pipelineSpec,
                    std::format("{}[{}]", spec.name, i),
                    spec.outputs,
                    spec.depthOutput
                );
                pipelines.emplace_back(std::move(pipeline));
            }

            return{
                .name = spec.name,
                .enabled = true,
                .outputs = spec.outputs,
                .depthOutput = spec.depthOutput,
                .pipelines = std::move(pipelines)
            };
        }

        GraphicsPipelineBind createPipeline(
            const GraphicsPipelineBindSpec& spec,
            std::string name,
            std::span<const std::string> outputs,
            std::string_view depthOutput
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
            auto pipeline = createPipelineStateHelper(
                spec, vs.get(), fs.get(), name,
                outputs, depthOutput
            );

            std::optional<RenderTypeHash> renderType;
            if(!spec.renderType.empty())
                renderType = std::hash<RenderType>{}(spec.renderType);

            return{
                .name = name,
                .inputs = spec.inputs,
                .fs_samplers = spec.fs_samplers,
                .fs_cbuffers = spec.fs_cbuffers,
                .pso = std::move(pipeline),
                .renderType = renderType
            };
        }

        RHIPipelineStatePtr createPipelineStateHelper(
            const GraphicsPipelineBindSpec& spec,
            RHIShader* vertexShader,
            RHIShader* fragmentShader,
            const std::string& name,
            std::span<const std::string> outputs,
            std::string_view depthOutput
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
                .renderTargetCount = static_cast<uint32_t>(outputs.size())
            };

            for(int i=0; i<outputs.size(); ++i){
                const auto& name = outputs[i];

                auto it = textures.find(name);
                if(it != textures.end())
                    desc.renderTargetFormats[i] = it->second->getFormat();
                else
                    // TODO. Backbuffer
                    desc.renderTargetFormats[i] = RHITextureFormat::BGRA8_UNORM;
            }

            if(desc.depthStencil.has_value()){
                auto it = textures.find(depthOutput);
                CROWY_ASSERT(it != textures.end());
                CROWY_ASSERT(it->second->getFormat() == desc.depthStencil->format);
            }

            return device->createGraphicsPipelineState(desc, name);
        }

        void loadComputePasses(
            std::span<const ComputePassSpec> specs
        ){
            for(const auto& spec: specs){
                computePassIndex[spec.name] = computePasses.size();
                computePasses.push_back(createPass(spec));
            }
        }

        ComputePass createPass(
            const ComputePassSpec& spec
        ){
            auto cs = device->createShader(RHIShaderCreateDesc{
                .file = spec.shader.filePath.c_str(),
                .entry = spec.shader.funcName.c_str(),
                .stage = RHIShaderStage::ComputeShader
            });
            auto pso = device->createComputePipelineState({
                .computeShader = cs.get(),
                .gridSize = spec.gridSize,
                .threadGroupSize = spec.threadGroupSize
            });

            return{
                .name = spec.name,
                .enabled = true,
                .inputTextures = spec.inputTextures,
                .inputBuffers = spec.inputBuffers,
                .outputTextures = spec.outputTextures,
                .outputBuffers = spec.outputBuffers,
                .pso = std::move(pso),
                .gridSize = spec.gridSize
            };
        }

    public:
        void addPass(
            const std::string& name,
            const std::vector<ResourceDesc>& resources,
            const std::vector<GraphicsPipelineBind>& pipelines
        ){

        }

        void compile(
            RenderGraphCompileOptions options = {}
        ){
            executionOrder.clear();

            // TODO. impl topological sort
            for(size_t i=0; i<renderPasses.size(); ++i){
                executionOrder.push_back({
                    .type = PassType::RenderPass,
                    .index = i
                });
            }
            for(size_t i=0; i<computePasses.size(); ++i){
                executionOrder.push_back({
                    .type = PassType::ComputePass,
                    .index = i
                });
            }
        }

        // execute render pass with immediate compile (used for initializing Texture)
        void instantExecute(
            const RenderPassSpec& passSpec,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* swapchain
        ){
            auto pass = createPass(passSpec);

            executePass(
                pass,
                cmdList,
                ctx,
                swapchain
            );
        }

        void execute(
            RHICommandList& cmdList,
            const RenderContext& ctx = {},
            RHISwapchain* swapchain = nullptr
        ){
            // reset Per-Frame Buffer
            vsPerObjectBuffers.reset();
            passParamBuffers.reset();

            for(const auto& index: executionOrder){
                if(index.type == PassType::RenderPass){
                    executePass(
                        renderPasses[index.index],
                        cmdList,
                        ctx,
                        swapchain
                    );
                }
                else{
                    executePass(
                        computePasses[index.index],
                        cmdList
                    );
                }
            }
        }

    private:
        void executePass(
            const RenderPass& pass,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* swapchain
        ){
            if(pass.enabled){
                RHIClearColor clearColor{0.2f, 0.2f, 0.3f, 0.0f};
                RHIClearDepthStencil clearDS = {1.0f, 0};
                RHIViewport viewport{
                    .x = 0.0f, .y = 0.0f,
                    .width = 0.0f, .height = 0.0f,
                    .minDepth = 0.0f, .maxDepth = 1.0f
                };

                CROWY_ASSERT(pass.outputs.size() > 0);
                const auto& renderTargetName = pass.outputs[0];

                RHITexture* depthTarget = nullptr;
                if(!pass.depthOutput.empty()){
                    auto depthTargetIt = textures.find(pass.depthOutput);
                    CROWY_ASSERT(depthTargetIt != textures.end());

                    depthTarget = depthTargetIt->second.get();
                }

                if(renderTargetName != "BackBuffer"){
                    std::vector<RHITexture*> renderTargets(pass.outputs.size());

                    // Multi Render Target support
                    for(size_t i=0; i<pass.outputs.size(); ++i){
                        const auto& targetName = pass.outputs[i];
                        CROWY_ASSERT(targetName != "BackBuffer");
                        auto targetIt = textures.find(targetName);
                        CROWY_ASSERT(targetIt != textures.end());

                        renderTargets[i] = targetIt->second.get();
                        if(i == 0){
                            viewport.width = targetIt->second->getWidth();
                            viewport.height = targetIt->second->getHeight();
                        }
                        else{
                            CROWY_ASSERT(viewport.width == targetIt->second->getWidth());
                            CROWY_ASSERT(viewport.height == targetIt->second->getHeight());
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
                        pass.name.c_str()
                    );
                }
                else{
                    viewport.width = swapchain->getWidth();
                    viewport.height = swapchain->getHeight();

                    cmdList.beginRenderPass(
                        *swapchain,
                        depthTarget,
                        RHILoadAction::Load,
                        RHIStoreAction::Store,
                        clearColor,
                        clearDS,
                        pass.name.c_str()
                    );
                }

                for(const auto& pipeline: pass.pipelines)
                    executePipeline(
                        pipeline,
                        cmdList,
                        ctx,
                        viewport
                    );

                cmdList.endRenderPass();
            }
            else{
                CROWY_ASSERT(pass.pipelines.size() > 0);

                const auto& pf = pass.pipelines.front();
                const auto& pb = pass.pipelines.back();

                // bypass for Post-Process, pf.inputs[0] for bypass target
                CROWY_ASSERT(pf.inputs.size() > 0);
                auto inputIt = textures.find(pf.inputs[0]);
                CROWY_ASSERT(inputIt != textures.end());

                // not support MRT for bypass target
                CROWY_ASSERT(pass.outputs.size() == 1);
                const auto& renderTargetName = pass.outputs[0];

                if(renderTargetName != "BackBuffer"){
                    auto outputIt = textures.find(renderTargetName);
                    CROWY_ASSERT(outputIt != textures.end());

                    cmdList.copy(*inputIt->second, *outputIt->second);
                }
                else{
                    cmdList.copy(*inputIt->second, *swapchain);
                }
            }
        }

        void executePass(
            const ComputePass& pass,
            RHICommandList& cmdList
        ){
            if(!pass.enabled)
                return;

            CROWY_ASSERT(pass.numOutputs() > 0);
            cmdList.beginCompute();

            using enum RHIShaderStage;

            cmdList.setPipelineState(pass.pso.get());

            for(const auto& bind: pass.inputTextures){
                auto it = textures.find(bind.name);
                CROWY_ASSERT(it != textures.end());

                cmdList.setTexture(
                    bind.slot,
                    *it->second,
                    ComputeShader
                );
            }
            for(const auto& bind: pass.inputBuffers){
                auto it = buffers.find(bind.name);
                CROWY_ASSERT(it != buffers.end());

                cmdList.setBuffer(
                    bind.slot,
                    *it->second,
                    ComputeShader
                );
            }

            for(const auto& bind: pass.outputTextures){
                auto it = textures.find(bind.name);
                CROWY_ASSERT(it != textures.end());

                cmdList.setTexture(
                    bind.slot,
                    *it->second,
                    ComputeShader
                );
            }
            for(const auto& bind: pass.outputBuffers){
                auto it = buffers.find(bind.name);
                CROWY_ASSERT(it != buffers.end());

                cmdList.setBuffer(
                    bind.slot,
                    *it->second,
                    ComputeShader
                );
            }

            cmdList.dispatch(pass.gridSize);

            cmdList.endCompute();
        }

        void executePipeline(
            const GraphicsPipelineBind& pipeline,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RHIViewport& viewport
        ){
            using enum RHIShaderStage;

            cmdList.setPipelineState(pipeline.pso.get());

            // bind input texture
            for(size_t i=0; i<pipeline.inputs.size(); ++i){
                auto srIt = textures.find(pipeline.inputs[i]);
                CROWY_ASSERT(srIt != textures.end());

                // transition input texture to shader resource state
                cmdList.transitionBarrier(
                    *srIt->second,
                    RHIResourceState::AllShaderResource
                );

                // TODO. select shader stage for advanced rendering technique
                cmdList.setTexture(
                    static_cast<uint32_t>(i),
                    *srIt->second,
                    FragmentShader
                );
            }

            for(const auto& samplerBind: pipeline.fs_samplers){
                auto it = samplers.find(samplerBind.name);
                CROWY_ASSERT(it != samplers.end());

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

    public:
        RHIBuffer* getBuffer(std::string_view bufferName){
            auto it = buffers.find(bufferName);
            if(it == buffers.end())
                return nullptr;

            return it->second.get();
        }

        CBuffer* getCBuffer(std::string_view cbufferName){
            auto it = cbuffers.find(cbufferName);
            if(it == cbuffers.end())
                return nullptr;

            return &it->second;
        }

        bool setPassEnabled(std::string_view passName, bool enabled){
            if(auto it = renderPassIndex.find(passName); it != renderPassIndex.end()){
                auto& pass = renderPasses[it->second];

                // TODO. check pass can be disabled
                pass.enabled = enabled;
                return true;
            }
            else if(auto it = computePassIndex.find(passName); it != computePassIndex.end()){
                auto& pass = computePasses[it->second];

                // TODO. check pass can be disabled
                pass.enabled = enabled;
                return true;
            }
            else{
                return false;
            }
        }
    };
}