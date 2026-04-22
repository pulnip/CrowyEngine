#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
#include <queue>
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

    struct DAG{
        std::vector<std::pair<size_t, size_t>> edges;
        std::unordered_map<size_t, std::vector<size_t>> adjacency;
    };

    class RenderGraph{
    private:
        RHIDevice* device = nullptr;

        // TODO. use pool for resource reuse and memory aliasing later.
        StringHashMap<RHIBufferPtr> buffers;
        StringHashMap<RHITexturePtr> textures;
        StringHashMap<RHISamplerPtr> samplers;
        StringHashMap<CBuffer> cbuffers;

        // ordered by execution order
        std::vector<RenderPass> renderPasses;
        std::vector<ComputePass> computePasses;
        // index of compute pass is right after renderPass
        StringHashMap<size_t> passIndex;

        std::vector<size_t> executionOrder;

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
                passIndex[spec.name] = renderPasses.size();
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
                .vs = spec.vs,
                .fs = spec.fs,
                .pso = std::move(pipeline),
                .renderType = renderType
            };
        }

        RHIGraphicsPipelineStatePtr createPipelineStateHelper(
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

            return device->createPipelineState(desc, name);
        }

        void loadComputePasses(
            std::span<const ComputePassSpec> specs
        ){
            for(const auto& spec: specs){
                passIndex[spec.name] = renderPasses.size() + computePasses.size();
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
            auto pso = device->createPipelineState({
                .computeShader = cs.get(),
                .gridSize = spec.gridSize,
                .threadGroupSize = spec.threadGroupSize
            });

            return{
                .name = spec.name,
                .enabled = true,
                .cs = spec.cs,
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

    private:
        static bool hasDirDeps(const RenderPass& from, const RenderPass& to){
            for(const auto& output: from.outputs){
                for(const auto& pip: to.pipelines){
                    auto it = std::ranges::find_if(
                        pip.fs.textures,
                        [&name = output](const auto& bind){
                            return bind.name == name;
                        }
                    );
                    if(it != pip.fs.textures.end())
                        return true;
                }
            }

            if(!from.depthOutput.empty()){
                for(const auto& pip: to.pipelines){
                    auto it = std::ranges::find_if(
                        pip.fs.textures,
                        [&name = from.depthOutput](const auto& bind){
                            return bind.name == name;
                        }
                    );
                    if(it != pip.fs.textures.end())
                        return true;
                }
            }

            return false;
        }

        static bool hasDirDeps(const RenderPass& from, const ComputePass& to){
            for(const auto& output: from.outputs){
                auto it = std::ranges::find_if(
                    to.cs.textures,
                    [&name = output](const auto& bind){
                        return bind.name == name;
                    }
                );
                if(it != to.cs.textures.end())
                    return true;
            }

            if(!from.depthOutput.empty()){
                auto it = std::ranges::find_if(
                    to.cs.textures,
                    [&name = from.depthOutput](const auto& bind){
                        return bind.name == name;
                    }
                );
                if(it != to.cs.textures.end())
                    return true;
            }

            return false;
        }

        static bool hasDirDeps(const ComputePass& from, const RenderPass& to){
            for(const auto& bind: from.cs.textures){
                for(const auto& pip: to.pipelines){
                    auto it = std::ranges::find_if(
                        pip.fs.textures,
                        [&name = bind.name](const auto& bind){
                            return bind.name == name;
                        }
                    );
                    if(it != pip.fs.textures.end())
                        return true;
                }
            }

            return false;
        }

        static bool hasDirDeps(const ComputePass& from, const ComputePass& to){
            for(const auto& bind: from.cs.textures){
                auto it = std::ranges::find_if(
                    to.cs.textures,
                    [&name = bind.name](const auto& bind){
                        return bind.name == name;
                    }
                );
                if(it != to.cs.textures.end())
                    return true;
            }

            return false;
        }

        static bool hasDeps(const RenderPass& p1, const RenderPass& p2){
            return hasDirDeps(p1, p2) || hasDirDeps(p2, p1);
        }

        static bool hasDeps(const RenderPass& p1, const ComputePass& p2){
            return hasDirDeps(p1, p2) || hasDirDeps(p2, p1);
        }

        static bool hasDeps(const ComputePass& p1, const RenderPass& p2){
            return hasDeps(p2, p1);
        }

        static bool hasDeps(const ComputePass& p1, const ComputePass& p2){
            return hasDirDeps(p1, p2) || hasDirDeps(p2, p1);
        }

        DAG makeDAG(){
            DAG dag;

            for(size_t i=0; i<renderPasses.size(); ++i){
                for(size_t j=i+1; j<renderPasses.size(); ++j){
                    if(hasDeps(renderPasses[i], renderPasses[j]))
                        dag.edges.push_back({i, j});
                }
                for(size_t j=0; j<computePasses.size(); ++j){
                    if(hasDeps(renderPasses[i], computePasses[j]))
                        dag.edges.push_back({i, renderPasses.size()+j});
                }
            }
            for(size_t i=0; i<computePasses.size(); ++i){
                for(size_t j=i+1; j<computePasses.size(); ++j){
                    if(hasDeps(computePasses[i], computePasses[j]))
                        dag.edges.push_back({
                            renderPasses.size() + i,
                            renderPasses.size() + j
                        });
                }
            }
            for(const auto& [from, to]: dag.edges){
                auto [it, res] = dag.adjacency.try_emplace(
                    from,
                    std::vector<size_t>{to}
                );

                if(!res){
                    it->second.emplace_back(to);
                }
            }

            return dag;
        }

    public:
        void compile(
            RenderGraphCompileOptions options = {}
        ){
            auto dag = makeDAG();
            executionOrder.clear();

            std::vector<size_t> inDegrees(numPasses(), 0);
            for(const auto& [from, to]: dag.edges){
                ++inDegrees[to];
            }

            std::queue<size_t> ready;
            // init ready from deps == 0
            for(size_t i=0; i<inDegrees.size(); ++i){
                if(inDegrees[i] == 0)
                    ready.push(i);
            }

            // topological sort from Kahn's algorithm
            while(!ready.empty()){
                auto current = ready.front();
                ready.pop();

                executionOrder.push_back(current);

                for(const auto& next: dag.adjacency[current]){
                    if(--inDegrees[next] == 0)
                        ready.push(next);
                }
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
                const auto isRenderPass = index < renderPasses.size();

                if(isRenderPass){
                    executePass(
                        renderPasses[index],
                        cmdList,
                        ctx,
                        swapchain
                    );
                }
                else{
                    executePass(
                        computePasses[index - renderPasses.size()],
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
                CROWY_ASSERT(pf.fs.textures.size() > 0);
                auto inputIt = textures.find(pf.fs.textures[0].name);
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

            cmdList.beginCompute();

            using enum RHIShaderStage;

            cmdList.setPipelineState(*pass.pso);
            auto& info = pass.pso->getInfo();

            for(const auto& bind: pass.cs.textures){
                auto it = textures.find(bind.name);
                CROWY_ASSERT(it != textures.end());

                cmdList.setTexture(
                    info.csInfo.textureInfo.at(bind.slot).index,
                    *it->second,
                    ComputeShader
                );
            }
            for(const auto& bind: pass.cs.buffers){
                auto it = buffers.find(bind.name);
                CROWY_ASSERT(it != buffers.end());

                cmdList.setBuffer(
                    info.csInfo.bufferInfo.at(bind.slot).index,
                    *it->second,
                    ComputeShader
                );
            }
            for(const auto& bind: pass.cs.bytes){
                cmdList.setBytes(
                    info.csInfo.bufferInfo.at(bind.slot).index,
                    &bind.data,
                    sizeof(bind.data),
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

            cmdList.setPipelineState(*pipeline.pso);
            auto& info = pipeline.pso->getInfo();

            // bind input texture
            for(size_t i=0; i<pipeline.fs.textures.size(); ++i){
                auto srIt = textures.find(pipeline.fs.textures[i].name);
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

            for(const auto& samplerBind: pipeline.fs.samplers){
                auto it = samplers.find(samplerBind.name);
                CROWY_ASSERT(it != samplers.end());

                cmdList.setSampler(
                    info.fsInfo.samplerInfo.at(samplerBind.slot).index,
                    *it->second,
                    FragmentShader
                );
            }

            for(const auto& cbufferBind: pipeline.fs.cbuffers){
                auto it = cbuffers.find(cbufferBind.name);
                [[unlikely]] if(it == cbuffers.end())
                    continue;

                const auto& cbuf = it->second;
                auto& buf = passParamBuffers.acquire();
                buf.upload(cbuf.buffer.data(), cbuf.buffer.size());

                cmdList.setConstantBuffer(
                    FragmentShader,
                    info.fsInfo.bufferInfo.at(cbufferBind.slot).index,
                    buf
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
        size_t numPasses() const{
            return renderPasses.size() + computePasses.size();
        }

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
            if(auto it = passIndex.find(passName); it != passIndex.end()){
                const auto isRenderPass = it->second < renderPasses.size();
                if(isRenderPass){
                    auto& pass = renderPasses[it->second];
                    // TODO. check pass can be disabled
                    pass.enabled = enabled;

                }
                else{
                    auto& pass = computePasses[it->second - renderPasses.size()];
                    // TODO. check pass can be disabled
                    pass.enabled = enabled;
                }

                return true;
            }

            return false;
        }
    };
}