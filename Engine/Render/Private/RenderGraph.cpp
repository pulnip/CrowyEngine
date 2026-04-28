#include <variant>
#include <queue>
#include "RHIDefinitions.hpp"
#include "Resource.hpp"
#include "ResourceHandle.hpp"
#include "RenderGraph.hpp"

namespace Crowy
{
    struct PerObjectParam{
        Mat4 mvp;
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
        RHIPixelFormat format = RHIPixelFormat::Unknown;
    };

    struct PipelineDesc{

    };

    RenderGraph::RenderGraph(RHIDevice& device)
        : device(device)
        , vsPerObjectBuffers(device, RHIBufferCreateDesc{
            .size = sizeof(PerObjectParam),
            .usage = RHIBufferUsage::ConstantBuffer,
            .access = RHIMemoryAccess::CPUWrite,
            .initialData = nullptr
        }, "Uniform Buffer")
        , passParamBuffers(device, RHIBufferCreateDesc{
            .size = 256,
            .usage = RHIBufferUsage::ConstantBuffer,
            .access = RHIMemoryAccess::CPUWrite,
            .initialData = nullptr
        }, "PassParam Buffer"){}

    void RenderGraph::loadPasses(
        const RenderSpec& spec,
        int screenWidth,
        int screenHeight
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

    void RenderGraph::loadBuffers(
        const BufferDescMap& descs
    ){
        for(const auto& [
            name,
            desc
        ]: descs){
            buffers.emplace(
                name,
                device.createBuffer(desc, name)
            );
        }
    }

    void RenderGraph::loadTextures(
        const TextureDescMap& descs,
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
                device.createTexture(resolved, name)
            );
        }
    }

    void RenderGraph::loadSamplers(
        const SamplerDescMap& descs
    ){
        for(const auto& [
            name,
            desc
        ]: descs){
            samplers.emplace(
                name,
                device.createSampler(desc)
            );
        }
    }

    void RenderGraph::loadCBuffers(
        const CBufferDescMap& cbufs
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

    void RenderGraph::loadRenderPasses(
        std::span<const RenderPassSpec> specs
    ){
        for(const auto& spec: specs){
            passIndex[spec.name] = renderPasses.size();
            renderPasses.push_back(createPass(spec));
        }
    }

    RenderPass RenderGraph::createPass(
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

    GraphicsPipelineBind RenderGraph::createPipeline(
        const GraphicsPipelineBindSpec& spec,
        std::string name,
        std::span<const std::string> outputs,
        std::string_view depthOutput
    ){
        auto pipeline = createPipelineStateHelper(
            spec, name,
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

    RHIGraphicsPipelineStateRAII RenderGraph::createPipelineStateHelper(
        const GraphicsPipelineBindSpec& spec,
        const std::string& name,
        std::span<const std::string> outputs,
        std::string_view depthOutput
    ){
        auto isFullscreen = spec.renderType.empty();

        RHIGraphicsPipelineStateDesc desc{
            // fullscreen pipeline doesn't need vertex layout
            .vertexLayout = std::nullopt,
            .vertexShaderPath = spec.shader.vsFilePath,
            .vertexShaderEntryPoint = spec.shader.vsFuncName,
            .rasterizer = spec.rasterizer,
            .fragmentShaderPath = spec.shader.fsFilePath,
            .fragmentShaderEntryPoint = spec.shader.fsFuncName,
            .depthStencil = spec.depthStencil,
            .blend = spec.blend,
            .renderTargetCount = static_cast<uint32_t>(outputs.size())
        };
        if(!isFullscreen)
            desc.vertexLayout = DEFAULT_VERTEX_ELEMENTS;

        for(int i=0; i<outputs.size(); ++i){
            const auto& name = outputs[i];

            auto it = textures.find(name);
            if(it != textures.end())
                desc.renderTargetFormats[i] = it->second->getFormat();
            else
                // TODO. Backbuffer
                desc.renderTargetFormats[i] = RHIPixelFormat::BGRA8_UNORM;
        }

        if(desc.depthStencil.has_value()){
            auto it = textures.find(depthOutput);
            CROWY_ASSERT(it != textures.end());
            CROWY_ASSERT(it->second->getFormat() == desc.depthStencil->format);
        }

        return device.createPipelineState(desc, name);
    }

    void RenderGraph::loadComputePasses(
        std::span<const ComputePassSpec> specs
    ){
        for(const auto& spec: specs){
            passIndex[spec.name] = renderPasses.size() + computePasses.size();
            computePasses.push_back(createPass(spec));
        }
    }

    ComputePass RenderGraph::createPass(
        const ComputePassSpec& spec
    ){
        auto pso = device.createPipelineState({
            .computeShaderPath = spec.shader.filePath,
            .computeShaderEntryPoint = spec.shader.funcName,
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

    DAG RenderGraph::makeDAG(){
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

    void RenderGraph::compile(
        RenderGraphCompileOptions options
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
    void RenderGraph::instantExecute(
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

    void RenderGraph::execute(
        RHICommandList& cmdList,
        const RenderContext& ctx,
        RHISwapchain* swapchain
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

    void RenderGraph::executePass(
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

    void RenderGraph::executePass(
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
                *it->second,
                info.csInfo.textureInfo.at(bind.slot).index,
                bind.access,
                ComputeShader
            );
        }
        for(const auto& bind: pass.cs.buffers){
            auto it = buffers.find(bind.name);
            CROWY_ASSERT(it != buffers.end());

            cmdList.setBuffer(
                *it->second,
                info.csInfo.bufferInfo.at(bind.slot).index,
                bind.access,
                ComputeShader
            );
        }
        for(const auto& bind: pass.cs.bytes){
            cmdList.setBytes(
                &bind.data,
                info.csInfo.bufferInfo.at(bind.slot).index,
                sizeof(bind.data),
                ComputeShader
            );
        }

        cmdList.dispatch(pass.gridSize);

        cmdList.endCompute();
    }

    void RenderGraph::executePipeline(
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
                *srIt->second,
                static_cast<uint32_t>(i),
                RHIBindingAccess::ReadOnly,
                FragmentShader
            );
        }

        for(const auto& samplerBind: pipeline.fs.samplers){
            auto it = samplers.find(samplerBind.name);
            CROWY_ASSERT(it != samplers.end());

            cmdList.setSampler(
                *it->second,
                info.fsInfo.samplerInfo.at(samplerBind.slot).index,
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
                buf,
                info.fsInfo.bufferInfo.at(cbufferBind.slot).index,
                FragmentShader
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
            drawFullscreenQuad(cmdList);
        }
        else{
            drawObjectsWithType(cmdList, ctx, *pipeline.renderType);
        }
    }

    void RenderGraph::drawObjectsWithType(
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

            cmdList.setConstantBuffer(buf, vsPerObjectCBufferSlot, VertexShader);

            for(const auto& submesh: mesh){
                // TODO. hide slot number (bc it's for Metal)
                cmdList.setVertexBuffer(*submesh.vertexBuffer, 0);
                cmdList.setIndexBuffer(*submesh.indexBuffer);

                auto it = materialSet.find(submesh.materialSlotName);
                if(it == materialSet.end())
                    continue;
                cmdList.setTexture(*it->second->baseColorMap, 0,
                    RHIBindingAccess::ReadOnly,
                    FragmentShader
                );

                cmdList.drawIndexed(submesh.indexCount, 1);
            }
        }
    }

    void RenderGraph::drawFullscreenQuad(
        RHICommandList& cmdList
    ){
        cmdList.draw(6, 1);
    }
}