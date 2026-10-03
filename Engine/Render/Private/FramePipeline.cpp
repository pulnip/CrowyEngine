#include "FramePipeline.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>

#include "Assert.hpp"
#include "EnumUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

namespace Crowy
{
    namespace
    {
        // one pass's use of one target, in list order
        struct TargetUse {
            usize pass = 0;
            RHIResourceUsage usage = RHIResourceUsage::Undefined;
            // an attachment's actions; a read loads and keeps
            RHILoadAction load = RHILoadAction::Load;
            RHIStoreAction store = RHIStoreAction::Store;
            bool read = false;
        };

        using TargetUses = std::vector<std::vector<TargetUse>>;

        // FullscreenPush's handles: source, then input0..2
        constexpr usize FullscreenReadCount = 4;

        [[noreturn]] void refuse(StrView pass, StrView rule) {
            throw std::invalid_argument(
                std::format("pass '{}': {}", pass, rule)
            );
        }

        RHITextureUsage textureUsageOf(RHIResourceUsage usage) {
            switch(usage) {
            case RHIResourceUsage::RenderTarget:
                return RHITextureUsage::RenderTarget;
            case RHIResourceUsage::DepthWrite:
                return RHITextureUsage::DepthStencil;
            case RHIResourceUsage::SampledFragment:
                return RHITextureUsage::ShaderResource;
            default:
                CROWY_ASSERT(false, "a frame target is attached or sampled");
                return RHITextureUsage::None;
            }
        }

        void refuseDuplicateNames(const FramePipelineDesc& desc) {
            for(usize i = 0; i < desc.targets.size(); ++i) {
                for(usize j = i + 1; j < desc.targets.size(); ++j) {
                    if(desc.targets[i].name == desc.targets[j].name) {
                        throw std::invalid_argument(
                            std::format(
                                "two targets are named '{}'",
                                desc.targets[i].name
                            )
                        );
                    }
                }
            }
            for(usize i = 0; i < desc.passes.size(); ++i) {
                for(usize j = i + 1; j < desc.passes.size(); ++j) {
                    if(desc.passes[i].name == desc.passes[j].name)
                        refuse(
                            desc.passes[i].name,
                            "two passes have this name"
                        );
                }
            }
            // a second acquire of a hook's buffers would pair with nothing
            for(usize i = 0; i < desc.passes.size(); ++i) {
                const auto* hook =
                    std::get_if<HookPassDesc>(&desc.passes[i].kind);
                if(hook == nullptr)
                    continue;
                for(usize j = i + 1; j < desc.passes.size(); ++j) {
                    const auto* other =
                        std::get_if<HookPassDesc>(&desc.passes[j].kind);
                    if(other != nullptr && other->hook == hook->hook) {
                        refuse(
                            desc.passes[j].name,
                            std::format(
                                "hook '{}' is bound by an earlier pass",
                                hook->hook
                            )
                        );
                    }
                }
            }
        }

        // every pass's uses, per target, after the rules a single pass keeps
        TargetUses collectUses(const FramePipelineDesc& desc) {
            const auto targetCount = desc.targets.size();
            const auto known = [targetCount](FrameTargetID id) {
                return id <= targetCount;
            };

            TargetUses uses(targetCount + 1);
            for(usize i = 0; i < desc.passes.size(); ++i) {
                const auto& pass = desc.passes[i];

                if(std::holds_alternative<FullscreenPassDesc>(pass.kind)) {
                    if(pass.colors.empty())
                        refuse(
                            pass.name,
                            "a fullscreen pass needs a color target"
                        );
                    if(pass.depth)
                        refuse(
                            pass.name,
                            "a fullscreen pass has no depth target"
                        );
                    if(pass.reads.size() > FullscreenReadCount) {
                        refuse(
                            pass.name,
                            std::format(
                                "a fullscreen pass reads at most {} targets",
                                FullscreenReadCount
                            )
                        );
                    }
                }

                std::vector<FrameTargetID> attached;
                const auto attach = [&](FrameTargetID id, TargetUse use) {
                    if(!known(id))
                        refuse(
                            pass.name,
                            std::format("target {} is unknown", id)
                        );
                    if(std::ranges::contains(attached, id)) {
                        refuse(
                            pass.name,
                            std::format("it attaches target {} twice", id)
                        );
                    }
                    attached.push_back(id);
                    uses[id].push_back(use);
                };

                for(const auto& color: pass.colors) {
                    attach(
                        color.target,
                        TargetUse{
                            .pass = i,
                            .usage = RHIResourceUsage::RenderTarget,
                            .load = color.load,
                            .store = color.store
                        }
                    );
                }
                if(pass.depth) {
                    if(pass.depth->target == BackBufferTarget)
                        refuse(
                            pass.name,
                            "the back buffer is not a depth target"
                        );
                    attach(
                        pass.depth->target,
                        TargetUse{
                            .pass = i,
                            .usage = RHIResourceUsage::DepthWrite,
                            .load = pass.depth->load,
                            .store = pass.depth->store
                        }
                    );
                }

                // a pass samples what an earlier one left, never what it
                // attaches itself
                std::vector<FrameTargetID> read;
                for(const auto id: pass.reads) {
                    if(!known(id))
                        refuse(
                            pass.name,
                            std::format("target {} is unknown", id)
                        );
                    if(std::ranges::contains(attached, id)) {
                        refuse(
                            pass.name,
                            std::format(
                                "it reads target {}, which it attaches",
                                id
                            )
                        );
                    }
                    if(std::ranges::contains(read, id)) {
                        refuse(
                            pass.name,
                            std::format("it reads target {} twice", id)
                        );
                    }
                    read.push_back(id);
                    uses[id].push_back(
                        TargetUse{
                            .pass = i,
                            .usage = RHIResourceUsage::SampledFragment,
                            .read = true
                        }
                    );
                }

                if(const auto* hook = std::get_if<HookPassDesc>(&pass.kind)) {
                    if(hook->hook.empty())
                        refuse(pass.name, "a hook pass names its hook");
                    if(pass.colors.empty())
                        refuse(pass.name, "a hook pass needs a color target");
                }

                if(const auto* mesh = std::get_if<MeshPassDesc>(&pass.kind)) {
                    // Compose always emits a depth state
                    if(!pass.depth)
                        refuse(pass.name, "a mesh pass needs a depth target");
                    if(pass.colors.empty() && mesh->state.fragmentShader) {
                        refuse(
                            pass.name,
                            "a depth-only mesh pass has no fragment stage to "
                            "replace"
                        );
                    }
                }
            }

            return uses;
        }

        // one triangle from the fragment shader's own file, which includes
        // Fullscreen.slang and so defines vs_main
        RHIGraphicsPipelineStateDesc fullscreenPipelineDesc(
            const FullscreenPassDesc& fullscreen,
            std::span<const RHIPixelFormat> formats
        ) {
            RHIGraphicsPipelineStateDesc desc{
                .preRasterizer =
                    RHILegacyFrontendDesc{
                        .vertexShader =
                            RHIShaderDesc{
                                .path = fullscreen.fragmentShader.path,
                                .entryPoint = "vs_main"
                            }
                    },
                .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader = fullscreen.fragmentShader,
                .blend = fullscreen.blend,
                .renderTargetCount = formats.size(),
                .profile = "sm_6_8"
            };
            std::ranges::copy(formats, desc.renderTargetFormats.begin());

            return desc;
        }

        // the rules that follow one target through the list
        void refuseBrokenUses(
            const FramePipelineDesc& desc,
            const TargetUses& uses
        ) {
            const auto nameOf = [&desc](FrameTargetID id) -> Str {
                return id == BackBufferTarget
                           ? Str{"the back buffer"}
                           : "'" + desc.targets[id - 1].name + "'";
            };

            if(uses[BackBufferTarget].empty())
                throw std::invalid_argument("no pass writes the back buffer");

            for(FrameTargetID id = 0; id < uses.size(); ++id) {
                const auto& targetUses = uses[id];
                for(usize k = 0; k < targetUses.size(); ++k) {
                    const auto& use = targetUses[k];
                    const auto& passName = desc.passes[use.pass].name;
                    const bool loads =
                        use.read || use.load == RHILoadAction::Load;

                    if(id == BackBufferTarget && use.read)
                        refuse(passName, "it reads the back buffer");
                    if(k == 0 && loads) {
                        refuse(
                            passName,
                            std::format(
                                "its first use of {} keeps contents nothing "
                                "wrote",
                                nameOf(id)
                            )
                        );
                    }
                    if(k > 0 && loads &&
                       targetUses[k - 1].store == RHIStoreAction::DontCare) {
                        refuse(
                            passName,
                            std::format(
                                "it keeps {}, which pass '{}' stored DontCare",
                                nameOf(id),
                                desc.passes[targetUses[k - 1].pass].name
                            )
                        );
                    }
                }
            }
        }
    }

    FramePipeline::~FramePipeline() {
        for(auto& texture: textures)
            device.Retire(std::move(texture));
    }

    FramePipeline::FramePipeline(
        RHIDevice& device,
        FramePipelineDesc desc,
        RHIPixelFormat backBufferFormat,
        u32 width,
        u32 height
    )
        : device(device),
          desc(std::move(desc)),
          backBufferFormat(backBufferFormat),
          width(width),
          height(height) {
        const auto& targets = this->desc.targets;
        const auto& passDescs = this->desc.passes;

        if(this->desc.sceneColor > targets.size()) {
            throw std::invalid_argument(
                std::format(
                    "sceneColor names target {}, which is unknown",
                    this->desc.sceneColor
                )
            );
        }
        if(const auto map = this->desc.shadowMap; map != 0) {
            if(map > targets.size()) {
                throw std::invalid_argument(
                    std::format(
                        "shadowMap names target {}, which is unknown",
                        map
                    )
                );
            }
            const auto& target = targets[map - 1];
            if(target.width == 0 || target.width != target.height) {
                throw std::invalid_argument(
                    std::format(
                        "shadowMap names target {}, which is not a "
                        "fixed-size square target",
                        map
                    )
                );
            }
        }
        refuseDuplicateNames(this->desc);
        const auto uses = collectUses(this->desc);
        refuseBrokenUses(this->desc, uses);

        overlayPass = uses[BackBufferTarget].back().pass;
        if(passDescs[overlayPass].colors.size() != 1) {
            refuse(
                passDescs[overlayPass].name,
                "the overlay pass needs exactly one color target"
            );
        }

        passes.resize(passDescs.size());
        stats.resize(passDescs.size());
        for(usize i = 0; i < passDescs.size(); ++i) {
            const auto& pass = passDescs[i];
            auto& compiled = passes[i];

            for(const auto& color: pass.colors) {
                compiled.colorFormats.push_back(
                    color.target == BackBufferTarget
                        ? this->backBufferFormat
                        : targets[color.target - 1].format
                );
            }
            if(pass.depth)
                compiled.depthFormat = targets[pass.depth->target - 1].format;
            if(const auto* mesh = std::get_if<MeshPassDesc>(&pass.kind)) {
                compiled.drawList = std::make_unique<DrawList>(
                    this->device,
                    mesh->drawCapacity
                );
                viewCount = std::max(viewCount, mesh->view + 1);
            } else if(const auto* fullscreen =
                          std::get_if<FullscreenPassDesc>(&pass.kind)) {
                compiled.fullscreenDesc = fullscreenPipelineDesc(
                    *fullscreen,
                    compiled.colorFormats
                );
            }

            stats[i].name = pass.name;
        }
        hookBindings.assign(passDescs.size(), nullptr);

        // consecutive uses pair, read after read too, so the next writer
        // stays behind every reader; a first use acquires across submissions
        usages.assign(targets.size() + 1, RHITextureUsage::None);
        lastUses.assign(targets.size() + 1, LastUse{});
        for(FrameTargetID id = 0; id < uses.size(); ++id) {
            const auto& targetUses = uses[id];
            if(targetUses.empty())
                continue;

            for(const auto& use: targetUses)
                usages[id] = combine(usages[id], textureUsageOf(use.usage));

            for(usize k = 1; k < targetUses.size(); ++k) {
                const CompiledBarrier edge{
                    .target = id,
                    .before = targetUses[k - 1].usage,
                    .after = targetUses[k].usage
                };
                passes[targetUses[k - 1].pass].releases.push_back(edge);
                passes[targetUses[k].pass].acquires.push_back(edge);
            }

            const auto& first = targetUses.front();
            const auto& last = targetUses.back();
            lastUses[id] = LastUse{.pass = last.pass, .usage = last.usage};
            if(id == BackBufferTarget) {
                // Present ordering is the swapchain's, so neither half pairs
                passes[first.pass].acquires.push_back(
                    CompiledBarrier{
                        .target = id,
                        .before = RHIResourceUsage::Undefined,
                        .after = first.usage
                    }
                );
                passes[last.pass].releases.push_back(
                    CompiledBarrier{
                        .target = id,
                        .before = last.usage,
                        .after = RHIResourceUsage::Present
                    }
                );
            } else {
                passes[first.pass].acquires.push_back(
                    CompiledBarrier{
                        .target = id,
                        .before = last.usage,
                        .after = first.usage,
                        .crossSubmission = true
                    }
                );
            }
        }

        textures.resize(targets.size() + 1);
        createTargets(false);
    }

    void FramePipeline::createTargets(bool swapchainSizedOnly) {
        const auto& targets = desc.targets;
        for(FrameTargetID id = 1; id <= targets.size(); ++id) {
            const auto& target = targets[id - 1];
            const bool swapchainSized = target.width == 0;
            if(usages[id] == RHITextureUsage::None)
                continue;
            if(swapchainSizedOnly && !swapchainSized)
                continue;

            auto created = device.CreateTexture(
                RHITextureCreateDesc{
                    .width = swapchainSized ? width : target.width,
                    .height = swapchainSized ? height : target.height,
                    .format = target.format,
                    .usage = usages[id],
                    .clearColor = target.clearColor,
                    .clearDepthStencil = {.depth = target.clearDepth}
                },
                target.name
            );
            // a frame in flight may still read the old one
            device.Retire(std::move(textures[id]));
            textures[id] = std::move(created);
        }
    }

    void FramePipeline::Resize(u32 width, u32 height) {
        this->width = width;
        this->height = height;
        createTargets(true);
    }

    void FramePipeline::Prepare(
        SceneRenderer& renderer,
        const RenderScene& scene,
        const MeshPassOverride& debug
    ) {
        for(usize i = 0; i < passes.size(); ++i) {
            auto& compiled = passes[i];
            const auto* mesh = std::get_if<MeshPassDesc>(&desc.passes[i].kind);
            if(mesh == nullptr) {
                if(compiled.fullscreenDesc) {
                    compiled.fullscreenPipeline =
                        &renderer.Pipelines().Resolve(*compiled.fullscreenDesc);
                }
                continue;
            }

            auto& list = *compiled.drawList;
            const bool depthOnly = compiled.colorFormats.empty();
            list.Build(
                scene,
                renderer.Visible(mesh->view),
                renderer.Pipelines(),
                PassPipelineDesc{
                    .renderTargetFormats = compiled.colorFormats,
                    .depthFormat = compiled.depthFormat,
                    .state = mesh->state,
                    // a depth-only pass keeps its state in every debug view
                    .debug = depthOnly ? MeshPassOverride{} : debug
                },
                mesh->filter,
                mesh->order
            );
            list.Upload();

            stats[i].draws = list.DrawCount();
            stats[i].runs = static_cast<u32>(list.RunCount());
            stats[i].triangles = list.TriangleCount();
        }
    }

    RHITexture& FramePipeline::texture(
        FrameTargetID id,
        const FrameInputs& inputs
    ) const {
        if(id == BackBufferTarget) {
            CROWY_ASSERT(
                inputs.backBuffer != nullptr,
                "Record without a back buffer"
            );

            return *inputs.backBuffer;
        }
        CROWY_ASSERT(textures[id] != nullptr);

        return *textures[id];
    }

    RHITextureBarrier FramePipeline::makeBarrier(
        const CompiledBarrier& half,
        const FrameInputs& inputs
    ) const {
        auto& target = texture(half.target, inputs);
        if(half.crossSubmission) {
            return MakeCrossSubmissionBarrier(
                target,
                half.before,
                half.after,
                /*discardContents=*/true
            );
        }

        return MakeBarrier(target, half.before, half.after);
    }

    bool FramePipeline::capturedAfter(
        usize pass,
        FrameTargetID id,
        const FrameInputs& inputs
    ) const noexcept {
        if(id == BackBufferTarget || lastUses[id].pass != pass)
            return false;

        return std::ranges::contains(
            inputs.captures,
            id,
            &TargetReadback::target
        );
    }

    void FramePipeline::Record(
        RHICommandList& cmdList,
        const SceneRenderer& renderer,
        FrameInputs& inputs
    ) {
        for(usize c = 0; c < inputs.captures.size(); ++c) {
            [[maybe_unused]] const auto id = inputs.captures[c].target;
            CROWY_ASSERT(
                id != BackBufferTarget && id < textures.size() &&
                    textures[id] != nullptr,
                "a capture names a target some pass uses"
            );
            // the blit's acquire and release are one edge per target
            CROWY_ASSERT(
                !std::ranges::contains(
                    inputs.captures.subspan(c + 1),
                    id,
                    &TargetReadback::target
                ),
                "a target is captured once per frame"
            );
        }
        bindHooks(inputs);

        for(usize i = 0; i < passes.size(); ++i) {
            const auto& pass = desc.passes[i];
            const auto& compiled = passes[i];
            const auto* mesh = std::get_if<MeshPassDesc>(&pass.kind);
            auto* hook = hookBindings[i];
            const bool overlay = i == overlayPass;

            cmdList.BeginEvent(pass.name.c_str());

            acquireScratch.clear();
            for(const auto& half: compiled.acquires)
                acquireScratch.push_back(makeBarrier(half, inputs));
            if(overlay)
                acquireScratch.append_range(inputs.overlayAcquires);
            if(hook != nullptr)
                acquireScratch.append_range(hook->textureAcquires);
            // a repeat is free on D3D12 and a second wait on one fence on Metal
            auto bufferAcquires = std::span<const RHIBufferBarrier>{};
            if(mesh != nullptr)
                bufferAcquires = inputs.geometryAcquires;
            else if(hook != nullptr)
                bufferAcquires = hook->bufferAcquires;

            colorScratch.clear();
            for(const auto& color: pass.colors) {
                // any other back-buffer clear is black, as App::Render's
                auto clearColor = Colors::Black;
                if(color.target == desc.sceneColor)
                    clearColor = inputs.sceneClear;
                else if(color.target != BackBufferTarget)
                    clearColor = desc.targets[color.target - 1].clearColor;
                // a capture reads what the last use leaves
                colorScratch.push_back(
                    RHIColorAttachment{
                        .texture = &texture(color.target, inputs),
                        .loadAction = color.load,
                        .storeAction = capturedAfter(i, color.target, inputs)
                                           ? RHIStoreAction::Store
                                           : color.store,
                        .clearColor = clearColor
                    }
                );
            }
            RHIRenderPassDesc passDesc{.colorAttachments = colorScratch};
            if(pass.depth) {
                const auto target = pass.depth->target;
                passDesc.depthAttachment = RHIDepthAttachment{
                    .texture = &texture(target, inputs),
                    .loadAction = pass.depth->load,
                    .storeAction = capturedAfter(i, target, inputs)
                                       ? RHIStoreAction::Store
                                       : pass.depth->store,
                    .clearDepthStencil = {
                        .depth = desc.targets[target - 1].clearDepth
                    }
                };
            }
#if defined(_DEBUG) || !defined(NDEBUG)
            passDesc.debugName = pass.name;
#endif

            cmdList.BeginRenderPass(passDesc, acquireScratch, bufferAcquires);

            // D3D12 carries both across the passes of a list, while a Metal
            // encoder starts at the attachment's size
            const auto& sized = colorScratch.empty()
                                    ? *passDesc.depthAttachment->texture
                                    : *colorScratch.front().texture;
            cmdList.SetViewport(
                RHIViewport{
                    .x = 0,
                    .y = 0,
                    .width = static_cast<f32>(sized.GetWidth()),
                    .height = static_cast<f32>(sized.GetHeight()),
                    .minDepth = 0,
                    .maxDepth = 1
                }
            );
            cmdList.SetScissorRect(
                RHIScissorRect{
                    .left = 0,
                    .top = 0,
                    .right = static_cast<i32>(sized.GetWidth()),
                    .bottom = static_cast<i32>(sized.GetHeight())
                }
            );

            if(mesh != nullptr) {
                const auto& list = *compiled.drawList;
                // Metal starts every pass with no push and no buffers
                renderer.BindView(cmdList, ViewConstantBufferSlot, mesh->view);
                auto push = list.Push(renderer.FramePush());
                push.vertices = inputs.vertices;
                // only a pass that reads the map has an edge that orders it
                // after the Shadow pass
                if(desc.shadowMap != 0 &&
                   std::ranges::contains(pass.reads, desc.shadowMap)) {
                    push.shadowMap =
                        texture(desc.shadowMap, inputs).GetReadableID();
                }
                if(inputs.bindMeshPass)
                    inputs.bindMeshPass(cmdList, push);
                else
                    cmdList.SetPushGraphicsConstants(push);
                list.Submit(cmdList, inputs.indices);
            } else if(hook != nullptr) {
                renderer.BindView(cmdList, ViewConstantBufferSlot, 0);
                readScratch.clear();
                for(const auto read: pass.reads) {
                    readScratch.push_back(
                        texture(read, inputs).GetReadableID()
                    );
                }
                stats[i].draws = hook->record(
                    cmdList,
                    HookPassContext{
                        .pass = pass.name,
                        .formats =
                            HookPassFormats{
                                .colors = compiled.colorFormats,
                                .depth = compiled.depthFormat
                            },
                        .reads = readScratch
                    }
                );
            } else {
                const auto& fullscreen =
                    std::get<FullscreenPassDesc>(pass.kind);
                cmdList.SetPipelineState(*compiled.fullscreenPipeline);
                renderer.BindView(cmdList, ViewConstantBufferSlot, 0);

                FullscreenPush push{.params = fullscreen.params};
                const std::array<u64*, FullscreenReadCount> handles{
                    &push.source,
                    &push.input0,
                    &push.input1,
                    &push.input2
                };
                for(usize r = 0; r < pass.reads.size(); ++r) {
                    *handles[r] =
                        texture(pass.reads[r], inputs).GetReadableID();
                }
                cmdList.SetPushGraphicsConstants(push);
                cmdList.Draw(3);
            }

            if(overlay && inputs.recordOverlay)
                inputs.recordOverlay(cmdList);

            releaseScratch.clear();
            for(const auto& half: compiled.releases)
                releaseScratch.push_back(makeBarrier(half, inputs));
            // this frame only: the capture blit acquires it after the list
            for(const auto& capture: inputs.captures) {
                const auto& last = lastUses[capture.target];
                if(last.pass != i)
                    continue;
                releaseScratch.push_back(MakeBarrier(
                    texture(capture.target, inputs),
                    last.usage,
                    RHIResourceUsage::CopySrc
                ));
            }
            cmdList.EndRenderPass(releaseScratch);

            cmdList.EndEvent();

            stats[i].barrierEdges = static_cast<u32>(
                acquireScratch.size() + bufferAcquires.size() +
                releaseScratch.size()
            );
        }

        if(!inputs.captures.empty())
            recordCaptures(cmdList, inputs);
    }

    // after the overlay pass, whose back-buffer release pairs with nothing;
    // each target's release is left unpaired, so D3D12 completes it at Close
    // back into the layout and sync the next frame's first acquire names
    void FramePipeline::recordCaptures(
        RHICommandList& cmdList,
        FrameInputs& inputs
    ) {
        const auto caps = device.GetCapabilities();

        acquireScratch.clear();
        releaseScratch.clear();
        for(const auto& capture: inputs.captures) {
            auto& source = texture(capture.target, inputs);
            const auto usage = lastUses[capture.target].usage;
            acquireScratch.push_back(
                MakeBarrier(source, usage, RHIResourceUsage::CopySrc)
            );
            releaseScratch.push_back(
                MakeBarrier(source, RHIResourceUsage::CopySrc, usage)
            );
        }

        cmdList.BeginBlitPass(acquireScratch);
        // Metal labels an open encoder only
        cmdList.BeginEvent("Capture");
        for(auto& capture: inputs.captures) {
            auto& source = texture(capture.target, inputs);
            capture.recorded = inputs.frame;
            capture.format = source.GetFormat();
            capture.width = source.GetWidth();
            capture.height = source.GetHeight();
            capture.rowPitch =
                GetReadbackRowPitch(capture.format, capture.width, caps);
            capture.buffer = device.CreateBuffer(
                RHIBufferCreateDesc{
                    .size = capture.rowPitch * capture.height,
                    .memory = RHIMemoryType::CPURead
                },
                capture.request.target
            );
            cmdList.Copy(source, *capture.buffer, 0, capture.rowPitch);
        }
        cmdList.EndEvent();
        cmdList.EndBlitPass(releaseScratch);
    }

    u32 FramePipeline::ShadowMapSize() const noexcept {
        if(desc.shadowMap == 0)
            return 0;

        return desc.targets[desc.shadowMap - 1].width;
    }

    std::optional<FrameTargetID> FramePipeline::FindTarget(
        StrView name
    ) const noexcept {
        for(FrameTargetID id = 1; id <= desc.targets.size(); ++id) {
            if(desc.targets[id - 1].name != name)
                continue;
            if(usages[id] == RHITextureUsage::None)
                return std::nullopt;

            return id;
        }

        return std::nullopt;
    }

    OverlayFormats FramePipeline::Overlay() const noexcept {
        return OverlayFormats{
            .color = backBufferFormat,
            .depth = passes[overlayPass].depthFormat
        };
    }

    std::optional<HookPassFormats> FramePipeline::FindHook(
        StrView hook
    ) const noexcept {
        for(usize i = 0; i < passes.size(); ++i) {
            const auto* bound = std::get_if<HookPassDesc>(&desc.passes[i].kind);
            if(bound != nullptr && bound->hook == hook) {
                return HookPassFormats{
                    .colors = passes[i].colorFormats,
                    .depth = passes[i].depthFormat
                };
            }
        }

        return std::nullopt;
    }

    void FramePipeline::bindHooks(FrameInputs& inputs) {
        std::ranges::fill(hookBindings, nullptr);
        for(auto& binding: inputs.hooks) {
            const auto found =
                std::ranges::find_if(desc.passes, [&](const PassDesc& pass) {
                    const auto* hook = std::get_if<HookPassDesc>(&pass.kind);
                    return hook != nullptr && hook->hook == binding.name;
                });
            if(found == desc.passes.end()) {
                throw std::invalid_argument(
                    std::format(
                        "hook '{}' is bound, but no pass in the list names it",
                        binding.name
                    )
                );
            }
            const auto index =
                static_cast<usize>(found - desc.passes.begin());
            if(hookBindings[index] != nullptr) {
                refuse(
                    found->name,
                    std::format("hook '{}' is bound twice", binding.name)
                );
            }
            if(!binding.record)
                refuse(found->name, "its hook's binding records nothing");
            hookBindings[index] = &binding;
        }

        for(usize i = 0; i < desc.passes.size(); ++i) {
            const auto* hook = std::get_if<HookPassDesc>(&desc.passes[i].kind);
            if(hook != nullptr && hookBindings[i] == nullptr) {
                refuse(
                    desc.passes[i].name,
                    std::format("hook '{}' has no binding", hook->hook)
                );
            }
        }
    }
}
