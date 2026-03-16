#include <cstddef>
#include <string>
#include <unordered_map>
#include <vector>
#include "RHIDefinitions.hpp"
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

    class Renderer::Impl{
    private:
        RHIDevice* device = nullptr;
        std::vector<RenderPass> passes;
        std::unordered_map<std::string, size_t, StringHash, std::equal_to<>> passIndex;
        RenderTargetPool renderTargetPool;

        // TODO. need to fit in appropriate slot
        static constexpr uint32_t vsPerObjectCBufferSlot = 1;
        LinearBufferAllocator vsPerObjectBuffers;
        LinearBufferAllocator passParamBuffers;

        struct CBufferIndex{
            std::string passName;
            size_t index;
        };
        std::unordered_map<std::string, CBufferIndex, StringHash, std::equal_to<>> cbufferIndexes;

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
            #if defined(_DEBUG) || !defined(NDEBUG)
                , .debugName = "Uniform Buffer"
            #endif
            })
            , passParamBuffers(*device, RHIBufferCreateDesc{
                .size = 256,
                .usage = combine(
                    RHIBufferUsage::ConstantBuffer,
                    RHIBufferUsage::CPUWrite
                ),
                .stride = 0,
                .initialData = nullptr
            #if defined(_DEBUG) || !defined(NDEBUG)
                , .debugName = "PassParam Buffer"
            #endif
            }){}
        ~Impl() = default;

        void loadPasses(const RenderSpec& spec, int screenWidth, int screenHeight){
            for(const auto& [name, renderTarget]: spec.renderTargets){
                if(name != "BackBuffer"){
                    auto texDesc = renderTarget;
                    // fill real width
                    texDesc.width  = texDesc.width  == 0 ?
                        screenWidth  : texDesc.width;
                    texDesc.height = texDesc.height == 0 ?
                        screenHeight : texDesc.height;

                    renderTargetPool.create(name, texDesc, *device);
                }
            }

            for(const auto& passSpec: spec.passes){
                for(size_t i = 0; i < passSpec.fs_cbuffers.size(); ++i){
                    const auto& cbuf = passSpec.fs_cbuffers[i];
                    cbufferIndexes.emplace(cbuf.name, CBufferIndex{
                        .passName = passSpec.name,
                        .index = i
                    });
                }

                const auto index = passes.size();
                passIndex[passSpec.name] = index;
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

            for(const auto& pass: passes){
                if(pass.enabled){
                    executePass(pass, cmdList, ctx, backBuffer);
                }
                else{
                    // bypass for Post-Process, input[0] for bypass target
                    CROWY_ASSERT(pass.inputs.size() > 0);
                    auto  inputTarget = renderTargetPool.get(pass.inputs[0]);
                    CROWY_ASSERT(inputTarget != nullptr);

                    // not support MRT for bypass target
                    CROWY_ASSERT(pass.targets.size() == 1);
                    const auto& renderTargetName = pass.targets[0];

                    if(renderTargetName != "BackBuffer"){
                        auto renderTarget = renderTargetPool.get(renderTargetName);
                        CROWY_ASSERT(renderTarget != nullptr);

                        cmdList.copy(*inputTarget, *renderTarget);
                    }
                    else{
                        cmdList.copy(*inputTarget, *backBuffer);
                    }
                }
            }
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
            auto it = cbufferIndexes.find(cbufferName);
            if(it == cbufferIndexes.end())
                return nullptr;

            const auto& info = it->second;
            auto i = passIndex.at(info.passName);
            return &passes[i].fs_cbuffers[info.index];
        }

    private:
        RHIPipelineStatePtr createGraphicsPipelineStateHelper(
            const RenderPassSpec& spec,
            RHIShader* vertexShader,
            RHIShader* fragmentShader
        ){
            // fullscreen pass (no renderType) doesn't need vertex layout
            bool isFullscreenPass = spec.renderType.empty();

            RHIGraphicsPipelineStateDesc desc{
                .vertexShader = vertexShader,
                .pixelShader = fragmentShader,
                .vertexLayout = isFullscreenPass ? RHIVertexLayout{} : DEFAULT_VERTEX_LAYOUT,
                .rasterizer = spec.rasterizer,
                .depthStencil = spec.depthStencil,
                .blend = spec.blend,
                .renderTargetCount = static_cast<uint32_t>(spec.targets.size())
            #if defined(_DEBUG) || !defined(NDEBUG)
                , .debugName = spec.name
            #endif
            };

            for(int i=0; i<spec.targets.size(); ++i){
                const auto& targetName = spec.targets[i];

                if(auto tex = renderTargetPool.get(targetName))
                    desc.renderTargetFormats[i] = tex->getFormat();
            }

            if(desc.depthStencil.has_value()){
                auto tex = renderTargetPool.get(spec.depthTarget);
                CROWY_ASSERT(tex != nullptr);
                CROWY_ASSERT(tex->getFormat() == desc.depthStencil->format);
            }

            return device->createGraphicsPipelineState(desc);
        }

        RenderPass createPass(
            const RenderPassSpec& spec
        ){
            std::vector<RHISamplerPtr> fs_samplers;
            fs_samplers.reserve(spec.fs_samplers.size());
            for(const auto& fs_sampler: spec.fs_samplers){
                auto sampler = device->createSampler(fs_sampler);
                fs_samplers.push_back(std::move(sampler));
            }

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
                spec, vs.get(), fs.get()
            );

            std::optional<RenderTypeHash> renderType = std::nullopt;
            if(!spec.renderType.empty())
                renderType = std::hash<RenderType>{}(spec.renderType);

            return RenderPass{
                .name = spec.name,
                .enabled = true,
                .inputs = spec.inputs,
                .targets = spec.targets,
                .depthTarget = spec.depthTarget,
                .fs_samplers = std::move(fs_samplers),
                .vs = std::move(vs), .fs = std::move(fs),
                .renderType = renderType,
                .pipeline = std::move(pipeline),
                .fs_cbuffers = spec.fs_cbuffers
            };
        }


        void executePass(
            const RenderPass& pass,
            RHICommandList& cmdList,
            const RenderContext& ctx,
            RHISwapchain* backBuffer
        ){
            using enum RHIShaderStage;

            RHIClearColor clearColor{0.2f, 0.2f, 0.3f, 0.0f};
            RHIClearDepthStencil clearDS = {1.0f, 0};

            CROWY_ASSERT(pass.targets.size() > 0);
            const auto& renderTargetName = pass.targets[0];
            auto depthTarget = renderTargetPool.get(pass.depthTarget);
            if(renderTargetName != "BackBuffer"){
                std::vector<RHITexture*> renderTargets;
                renderTargets.reserve(pass.targets.size());

                // Multi Render Target support
                for(const auto& renderTargetName: pass.targets){
                    CROWY_ASSERT(renderTargetName != "BackBuffer");
                    auto renderTarget = renderTargetPool.get(renderTargetName);
                    CROWY_ASSERT(renderTarget != nullptr);

                    renderTargets.push_back(renderTarget);
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
                cmdList.beginRenderPass(
                    *backBuffer,
                    depthTarget,
                    RHILoadAction::Load,
                    RHIStoreAction::Store,
                    clearColor,
                    clearDS,
                    pass.name.c_str()
                );
            }
            cmdList.setPipelineState(pass.pipeline.get());

            // bind input texture
            for(size_t i=0; i<pass.inputs.size(); ++i){
                auto inputTarget = renderTargetPool.get(pass.inputs[i]);
                if(inputTarget == nullptr)
                    continue;

                // transition input texture to shader resource state
                cmdList.transitionBarrier(
                    *inputTarget,
                    RHIResourceState::AllShaderResource
                );

                // TODO. select shader stage for advanced rendering technique
                cmdList.setTexture(
                    static_cast<uint32_t>(i),
                    *inputTarget,
                    FragmentShader
                );
            }
            for(size_t i=0; i<pass.fs_samplers.size(); ++i){
                auto sampler = pass.fs_samplers[i].get();

                cmdList.setSampler(
                    static_cast<uint32_t>(i), *sampler,
                    FragmentShader
                );
            }

            for(auto& cbuf: pass.fs_cbuffers){
                auto& buf = passParamBuffers.acquire();

                buf.upload(cbuf.buffer.data(), cbuf.buffer.size());

                cmdList.setConstantBuffer(FragmentShader, cbuf.slot, buf);
            }

            cmdList.setViewport(ctx.viewport);
            cmdList.setScissorRect(RHIScissorRect{
                .left   = static_cast<int32_t>(ctx.viewport.x     ),
                .top    = static_cast<int32_t>(ctx.viewport.y     ),
                .right  = static_cast<int32_t>(ctx.viewport.width ),
                .bottom = static_cast<int32_t>(ctx.viewport.height)
            });

            // draw
            if(pass.isFullscreenPass()){
                drawFullscreenQuad(cmdList, ctx, pass.name);
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
            const RenderContext& ctx,
            std::string_view passName
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
