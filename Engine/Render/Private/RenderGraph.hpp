#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <utility>
#include <vector>
#include "ComputePass.hpp"
#include "LinearAllocator.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RenderSpec.hpp"
#include "RenderPass.hpp"
#include "RenderContext.hpp"
#include "RenderSpec.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHISampler.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    template<typename T>
    using StringHashMap = std::unordered_map<std::string, T, StringHash, std::equal_to<>>;

    struct RenderGraphCompileOptions{
        // TODO. not used for now.
        bool strictMemoryAliasing = false;
    };

    struct DAG{
        std::vector<std::pair<size_t, size_t>> edges;
        std::unordered_map<size_t, std::vector<size_t>> adjacency;
    };

    class RenderGraph{
    private:
        RHIDevice& device;

        // TODO. use pool for resource reuse and memory aliasing later.
        StringHashMap<RHIBufferRAII> buffers;
        StringHashMap<RHITextureRAII> textures;
        StringHashMap<RHISamplerRAII> samplers;
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
        RenderGraph(RHIDevice& device);
        ~RenderGraph() = default;

        void loadPasses(
            const RenderSpec& spec,
            int screenWidth = 0,
            int screenHeight = 0
        );

    private:
        using BufferDescMap = std::unordered_map<std::string, RHIBufferCreateDesc>;
        using TextureDescMap = std::unordered_map<std::string, RHITextureCreateDesc>;
        using SamplerDescMap = std::unordered_map<std::string, RHISamplerState>;
        using CBufferDescMap = std::unordered_map<std::string, CBuffer>;

        void loadBuffers(const BufferDescMap& descs);
        void loadTextures(const TextureDescMap& descs,
            int screenWidth, int screenHeight
        );
        void loadSamplers(const SamplerDescMap& descs);
        void loadCBuffers(const CBufferDescMap& cbufs);

        void loadRenderPasses(std::span<const RenderPassSpec> specs);
        RenderPass createPass(const RenderPassSpec& spec);
        GraphicsPipelineBind createPipeline(
            const GraphicsPipelineBindSpec& spec,
            std::string name,
            std::span<const std::string> outputs,
            std::string_view depthOutput
        );
        RHIGraphicsPipelineStateRAII createPipelineStateHelper(
            const GraphicsPipelineBindSpec& spec,
            const std::string& name,
            std::span<const std::string> outputs,
            std::string_view depthOutput
        );

        void loadComputePasses(std::span<const ComputePassSpec> specs);
        ComputePass createPass(const ComputePassSpec& spec);

    public:
        // void addPass(
        //     const std::string& name,
        //     const std::vector<ResourceDesc>& resources,
        //     const std::vector<GraphicsPipelineBind>& pipelines
        // ){

        // }

    private:
        DAG makeDAG();

    public:
        void compile(RenderGraphCompileOptions options = {});

        // execute render pass with immediate compile (used for initializing Texture)
        void instantExecute(
            const RenderPassSpec& passSpec,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* swapchain
        );

        void execute(
            RHICommandList& cmdList,
            const RenderContext& ctx = {},
            RHISwapchain* swapchain = nullptr
        );

    private:
        void executePass(
            const RenderPass& pass,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* swapchain
        );

        void executePass(
            const ComputePass& pass,
            RHICommandList& cmdList
        );

        void executePipeline(
            const GraphicsPipelineBind& pipeline,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RHIViewport& viewport
        );

        void drawObjectsWithType(
            RHICommandList& cmdList,
            const RenderContext& ctx,
            const RenderTypeHash& type
        );

        static void drawFullscreenQuad(
            RHICommandList& cmdList
        );

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