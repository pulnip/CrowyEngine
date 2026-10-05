#pragma once

#include <cstddef>
#include <cstring>
#include <format>
#include <optional>
#include <span>
#include <unordered_map>
#include <vector>

#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    // what the backends check when they pair barrier halves
    enum class FakeRule : u8 {
        // an acquire with real producer work and no crossSubmission equals
        // no parked release, or only one older than its texture's last
        // attachment: identical edge values would pair it past that pass
        UnmatchedAcquire,
        // a range released again while its earlier release is unconsumed
        StaleRelease,
        // an attachment with no acquire of it at its pass's begin, or one
        // into another usage: only that acquire's fence orders a Metal encoder
        UnacquiredAttachment,
        // a half whose layoutBefore is not the texture's layout
        Layout,
        // a draw before its pass set a push and slot 0: Metal starts every
        // encoder empty
        FreshBindings,
        // an attachment that loads while its acquire discards
        DiscardedLoad,
    };

    struct FakeViolation {
        FakeRule rule = FakeRule::UnmatchedAcquire;
        // the pass it happened in, counted from the list's Begin
        usize pass = 0;
        Str what;
    };

    // Records every call, mirrors how both backends pair barrier halves, and
    // then calls the base, so a pass-state misuse still aborts in Debug. A
    // broken pairing rule is a violation, never an abort, so a test can break
    // one on purpose.
    class FakeCommandList final: public RHICommandList {
    public:
        struct ConstantBufferSet {
            RHIBuffer* buffer = nullptr;
            u32 slot = 0;
            u32 offset = 0;
        };

        using Bytes = std::vector<std::byte>;
        using ColorAttachments = std::vector<RHIColorAttachment>;
        using TextureBarriers = std::vector<RHITextureBarrier>;
        using BufferBarriers = std::vector<RHIBufferBarrier>;
        using DrawBatches = std::vector<DrawBatchIndexed>;
        using Pushes = std::vector<Bytes>;
        using ConstantBufferSets = std::vector<ConstantBufferSet>;
        using Log = std::vector<Str>;

        struct RecordedPass {
            // the event open when it began
            Str event;
            ColorAttachments colors;
            std::optional<RHIDepthAttachment> depth;
            TextureBarriers acquires;
            BufferBarriers bufferAcquires;
            TextureBarriers releases;
            BufferBarriers bufferReleases;
            std::optional<RHIViewport> viewport;
            std::optional<RHIScissorRect> scissor;
            Pushes pushes;
            ConstantBufferSets constantBuffers;
            DrawBatches batches;
            u32 directDraws = 0;
            // "push", "cb <slot>", "draw", "marker <name>", in call order
            Log log;
        };

        struct RecordedCopy {
            RHITexture* src = nullptr;
            RHIBuffer* dst = nullptr;
            u64 dstOffset = 0;
            u32 dstRowPitch = 0;
            RHITextureRegion region;
        };

        using RecordedCopies = std::vector<RecordedCopy>;

        struct RecordedBlitPass {
            // the event opened inside it
            Str event;
            // how many render passes this list recorded before it
            usize after = 0;
            TextureBarriers acquires;
            BufferBarriers bufferAcquires;
            TextureBarriers releases;
            BufferBarriers bufferReleases;
            RecordedCopies copies;
        };

        struct RecordedDispatch {
            RHIComputePipelineState* pipeline = nullptr;
            // threads, not groups
            Size3D threads;
            // the push set last before it
            Bytes push;
        };

        using RecordedDispatches = std::vector<RecordedDispatch>;
        using DispatchBarriers = std::vector<BufferBarriers>;

        struct RecordedComputePass {
            // the event open when it began
            Str event;
            // how many render passes this list recorded before it
            usize after = 0;
            TextureBarriers acquires;
            BufferBarriers bufferAcquires;
            TextureBarriers releases;
            BufferBarriers bufferReleases;
            RecordedDispatches dispatches;
            // per DispatchBarrier, its buffer halves
            DispatchBarriers dispatchBarriers;
            // "pipeline", "push", "cb <slot>", "dispatch", "barrier", in call
            // order
            Log log;
        };

        using RecordedPasses = std::vector<RecordedPass>;
        using RecordedBlitPasses = std::vector<RecordedBlitPass>;
        using RecordedComputePasses = std::vector<RecordedComputePass>;
        using Violations = std::vector<FakeViolation>;

    private:
        template<typename Barrier>
        struct Parked {
            Barrier barrier;
            bool consumed = false;
            // the pass whose end released it
            usize pass = 0;
        };

        using ParkedTextures = std::
            unordered_map<RHITexture*, std::vector<Parked<RHITextureBarrier>>>;
        using ParkedBuffers =
            std::unordered_map<RHIBuffer*, Parked<RHIBufferBarrier>>;
        using Layouts = std::unordered_map<RHITexture*, RHITextureLayout>;
        using AttachedPasses = std::unordered_map<RHITexture*, usize>;

    public:
        // this list's passes since its last Begin
        RecordedPasses passes;
        RecordedBlitPasses blitPasses;
        RecordedComputePasses computePasses;
        // every broken rule since construction
        Violations violations;
        // the releases nothing consumed, listed at the last Close
        TextureBarriers unconsumedAtClose;
        BufferBarriers unconsumedBuffersAtClose;

    private:
        // layouts outlive a Begin: a texture starts Undefined and keeps what
        // the last transition left it in
        Layouts layouts;
        ParkedTextures parkedTextures;
        ParkedBuffers parkedBuffers;
        // each texture's latest pass as an attachment, since Begin
        AttachedPasses lastAttached;
        Str openEvent;
        RHIComputePipelineState* computePipeline = nullptr;
        Bytes computePush;
        bool inBlitPass = false;
        bool pushSet = false;
        bool viewSet = false;

    public:
        using RHICommandList::Copy;
        using RHICommandList::SetComputeConstantBuffer;
        using RHICommandList::SetGraphicsConstantBuffer;
        using RHICommandList::SetPipelineState;
        using RHICommandList::SetPushComputeConstants;
        using RHICommandList::SetPushGraphicsConstants;

        RHITextureLayout LayoutOf(RHITexture* texture) const {
            const auto found = layouts.find(texture);

            return found == layouts.end() ? RHITextureLayout::Undefined
                                          : found->second;
        }

        bool Violated(FakeRule rule) const {
            for(const auto& violation: violations) {
                if(violation.rule == rule)
                    return true;
            }

            return false;
        }

        void Begin() override {
            RHICommandList::Begin();

            passes.clear();
            blitPasses.clear();
            computePasses.clear();
            parkedTextures.clear();
            parkedBuffers.clear();
            lastAttached.clear();
        }

        void Close() override {
            RHICommandList::Close();

            // D3D12 completes each at Close; Metal parks it for the next
            // submission, which only a cross-submission acquire waits on
            unconsumedAtClose.clear();
            unconsumedBuffersAtClose.clear();
            for(auto& [texture, entries]: parkedTextures) {
                for(const auto& entry: entries) {
                    if(entry.consumed)
                        continue;
                    unconsumedAtClose.push_back(entry.barrier);
                    layouts[texture] = entry.barrier.layoutAfter;
                }
            }
            for(const auto& [_, entry]: parkedBuffers) {
                if(!entry.consumed)
                    unconsumedBuffersAtClose.push_back(entry.barrier);
            }
            parkedTextures.clear();
            parkedBuffers.clear();
        }

        void BeginRenderPass(
            const RHIRenderPassDesc& desc,
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) override {
            auto& pass = passes.emplace_back();
            pass.event = openEvent;
            pass.colors.assign_range(desc.colorAttachments);
            pass.depth = desc.depthAttachment;
            pass.acquires.assign_range(textureAcquires);
            pass.bufferAcquires.assign_range(bufferAcquires);
            pushSet = false;
            viewSet = false;

            acquire(textureAcquires, bufferAcquires);
            for(const auto& color: desc.colorAttachments) {
                lastAttached[color.texture] = passes.size() - 1;
                checkAttachment(
                    color.texture,
                    color.loadAction,
                    RHITextureLayout::RenderTarget,
                    textureAcquires
                );
            }
            if(desc.depthAttachment) {
                lastAttached[desc.depthAttachment->texture] = passes.size() - 1;
                checkAttachment(
                    desc.depthAttachment->texture,
                    desc.depthAttachment->loadAction,
                    RHITextureLayout::DepthWrite,
                    textureAcquires
                );
            }

            RHICommandList::BeginRenderPass(
                desc,
                textureAcquires,
                bufferAcquires
            );
        }

        void EndRenderPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) override {
            auto& pass = passes.back();
            pass.releases.assign_range(textureReleases);
            pass.bufferReleases.assign_range(bufferReleases);

            release(textureReleases, bufferReleases);

            RHICommandList::EndRenderPass(textureReleases, bufferReleases);
        }

        void BeginBlitPass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) override {
            auto& pass = blitPasses.emplace_back();
            pass.after = passes.size();
            pass.acquires.assign_range(textureAcquires);
            pass.bufferAcquires.assign_range(bufferAcquires);
            inBlitPass = true;

            acquire(textureAcquires, bufferAcquires);

            RHICommandList::BeginBlitPass(textureAcquires, bufferAcquires);
        }

        void EndBlitPass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) override {
            auto& pass = blitPasses.back();
            pass.releases.assign_range(textureReleases);
            pass.bufferReleases.assign_range(bufferReleases);
            inBlitPass = false;

            release(textureReleases, bufferReleases);

            RHICommandList::EndBlitPass(textureReleases, bufferReleases);
        }

        void BeginComputePass(
            std::span<const RHITextureBarrier> textureAcquires = {},
            std::span<const RHIBufferBarrier> bufferAcquires = {}
        ) override {
            auto& pass = computePasses.emplace_back();
            pass.event = openEvent;
            pass.after = passes.size();
            pass.acquires.assign_range(textureAcquires);
            pass.bufferAcquires.assign_range(bufferAcquires);
            computePipeline = nullptr;
            computePush.clear();

            acquire(textureAcquires, bufferAcquires);

            RHICommandList::BeginComputePass(textureAcquires, bufferAcquires);
        }

        void EndComputePass(
            std::span<const RHITextureBarrier> textureReleases = {},
            std::span<const RHIBufferBarrier> bufferReleases = {}
        ) override {
            auto& pass = computePasses.back();
            pass.releases.assign_range(textureReleases);
            pass.bufferReleases.assign_range(bufferReleases);

            release(textureReleases, bufferReleases);

            RHICommandList::EndComputePass(textureReleases, bufferReleases);
        }

        void SetPipelineState(RHIComputePipelineState& pipeline) override {
            computePipeline = &pipeline;
            if(!computePasses.empty())
                computePasses.back().log.push_back("pipeline");

            RHICommandList::SetPipelineState(pipeline);
        }

        void SetPushComputeConstants(const void* data, u32 size) override {
            computePush.resize(size);
            std::memcpy(computePush.data(), data, size);
            if(!computePasses.empty())
                computePasses.back().log.push_back("push");

            RHICommandList::SetPushComputeConstants(data, size);
        }

        void SetComputeConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) override {
            if(!computePasses.empty())
                computePasses.back().log.push_back(std::format("cb {}", slot));

            RHICommandList::SetComputeConstantBuffer(buffer, slot, offset);
        }

        void Dispatch(Size3D threads) override {
            if(!computePasses.empty()) {
                auto& pass = computePasses.back();
                pass.dispatches.push_back(
                    RecordedDispatch{
                        .pipeline = computePipeline,
                        .threads = threads,
                        .push = computePush
                    }
                );
                pass.log.push_back("dispatch");
            }

            RHICommandList::Dispatch(threads);
        }

        void DispatchBarrier(
            std::span<const RHITextureBarrier> textureBarriers = {},
            std::span<const RHIBufferBarrier> bufferBarriers = {}
        ) override {
            if(!computePasses.empty()) {
                auto& pass = computePasses.back();
                pass.dispatchBarriers.emplace_back().assign_range(
                    bufferBarriers
                );
                pass.log.push_back("barrier");
            }

            RHICommandList::DispatchBarrier(textureBarriers, bufferBarriers);
        }

        void Copy(
            RHITexture& src,
            RHIBuffer& dst,
            u64 dstOffset,
            u32 dstRowPitch,
            const RHITextureRegion& region,
            u32 mipLevel = 0,
            u32 arraySlice = 0
        ) override {
            if(LayoutOf(&src) != RHITextureLayout::CopySrc) {
                violate(
                    FakeRule::Layout,
                    "a copy reads a texture that is not in CopySrc"
                );
            }
            if(!blitPasses.empty()) {
                blitPasses.back().copies.push_back(
                    RecordedCopy{
                        .src = &src,
                        .dst = &dst,
                        .dstOffset = dstOffset,
                        .dstRowPitch = dstRowPitch,
                        .region = region
                    }
                );
            }

            RHICommandList::Copy(
                src,
                dst,
                dstOffset,
                dstRowPitch,
                region,
                mipLevel,
                arraySlice
            );
        }

        void SetPushGraphicsConstants(const void* data, u32 size) override {
            if(!passes.empty()) {
                auto& pass = passes.back();
                auto& bytes = pass.pushes.emplace_back(size);
                std::memcpy(bytes.data(), data, size);
                pass.log.push_back("push");
            }
            pushSet = true;

            RHICommandList::SetPushGraphicsConstants(data, size);
        }

        void SetGraphicsConstantBuffer(
            RHIBuffer& buffer,
            u32 slot,
            u32 offset = 0
        ) override {
            if(!passes.empty()) {
                auto& pass = passes.back();
                pass.constantBuffers.push_back(
                    ConstantBufferSet{
                        .buffer = &buffer,
                        .slot = slot,
                        .offset = offset
                    }
                );
                pass.log.push_back(std::format("cb {}", slot));
            }
            if(slot == 0)
                viewSet = true;

            RHICommandList::SetGraphicsConstantBuffer(buffer, slot, offset);
        }

        void SetViewport(const RHIViewport& viewport) override {
            if(!passes.empty())
                passes.back().viewport = viewport;

            RHICommandList::SetViewport(viewport);
        }

        void SetScissorRect(const RHIScissorRect& scissor) override {
            if(!passes.empty())
                passes.back().scissor = scissor;

            RHICommandList::SetScissorRect(scissor);
        }

        void Draw(
            u32 vertexCount,
            u32 instanceCount = 1,
            u32 startVertex = 0,
            u32 startInstance = 0
        ) override {
            checkBindings();
            if(!passes.empty()) {
                ++passes.back().directDraws;
                passes.back().log.push_back("draw");
            }

            RHICommandList::Draw(
                vertexCount,
                instanceCount,
                startVertex,
                startInstance
            );
        }

        void ExecuteIndirectIndexed(const DrawBatchIndexed& batch) override {
            checkBindings();
            if(!passes.empty()) {
                passes.back().batches.push_back(batch);
                passes.back().log.push_back("draw");
            }

            RHICommandList::ExecuteIndirectIndexed(batch);
        }

        void BeginEvent(CStr name) override {
            openEvent = name;
            if(inBlitPass)
                blitPasses.back().event = name;
        }
        void EndEvent() override { openEvent.clear(); }
        void SetMarker(CStr name) override {
            if(!passes.empty())
                passes.back().log.push_back(std::format("marker {}", name));
        }

    private:
        // a blit pass counts as the render pass before it
        usize currentPass() const {
            return passes.empty() ? 0 : passes.size() - 1;
        }

        void violate(FakeRule rule, Str what) {
            violations.push_back(
                FakeViolation{
                    .rule = rule,
                    .pass = currentPass(),
                    .what = std::move(what)
                }
            );
        }

        void acquire(
            std::span<const RHITextureBarrier> textureAcquires,
            std::span<const RHIBufferBarrier> bufferAcquires
        ) {
            for(const auto& half: textureAcquires) {
                auto* parked = findParked(half);
                if(parked != nullptr) {
                    const auto attached = lastAttached.find(half.texture);
                    if(attached != lastAttached.end() &&
                       parked->pass < attached->second) {
                        violate(
                            FakeRule::UnmatchedAcquire,
                            "a texture acquire pairs with a release older "
                            "than the texture's last attachment"
                        );
                    }
                    // an identical second acquire matches the same release
                    if(!parked->consumed) {
                        parked->consumed = true;
                        layouts[half.texture] = half.layoutAfter;
                    }
                    continue;
                }

                if(half.syncBefore != RHIBarrierSync::None &&
                   !half.crossSubmission) {
                    violate(
                        FakeRule::UnmatchedAcquire,
                        "a texture acquire matches no release"
                    );
                }
                const auto layout = LayoutOf(half.texture);
                const bool discards =
                    half.discard &&
                    half.layoutBefore == RHITextureLayout::Undefined;
                if(half.layoutBefore != layout && !discards) {
                    violate(
                        FakeRule::Layout,
                        "an acquire names a layout the texture is not in"
                    );
                }
                layouts[half.texture] = half.layoutAfter;
            }

            for(const auto& half: bufferAcquires) {
                const auto found = parkedBuffers.find(half.buffer);
                if(found != parkedBuffers.end() &&
                   found->second.barrier == half) {
                    found->second.consumed = true;
                    continue;
                }
                if(half.syncBefore != RHIBarrierSync::None &&
                   !half.crossSubmission) {
                    violate(
                        FakeRule::UnmatchedAcquire,
                        "a buffer acquire matches no release"
                    );
                }
            }
        }

        void release(
            std::span<const RHITextureBarrier> textureReleases,
            std::span<const RHIBufferBarrier> bufferReleases
        ) {
            for(const auto& half: textureReleases) {
                if(half.layoutBefore != LayoutOf(half.texture)) {
                    violate(
                        FakeRule::Layout,
                        "a release names a layout the texture is not in"
                    );
                }
                // self-contained, like RenderTarget -> Present
                if(half.syncAfter == RHIBarrierSync::None) {
                    layouts[half.texture] = half.layoutAfter;
                    continue;
                }

                auto& entries = parkedTextures[half.texture];
                bool replaced = false;
                for(auto& entry: entries) {
                    if(entry.barrier.range != half.range)
                        continue;
                    if(!entry.consumed) {
                        violate(
                            FakeRule::StaleRelease,
                            "a range is released again before anything "
                            "acquired its earlier release"
                        );
                        // D3D12 completes the earlier one here
                        layouts[half.texture] = entry.barrier.layoutAfter;
                    }
                    entry = Parked<RHITextureBarrier>{
                        .barrier = half,
                        .pass = currentPass()
                    };
                    replaced = true;
                }
                if(!replaced)
                    entries.push_back(
                        Parked<RHITextureBarrier>{
                            .barrier = half,
                            .pass = currentPass()
                        }
                    );
            }

            for(const auto& half: bufferReleases) {
                const auto found = parkedBuffers.find(half.buffer);
                if(found != parkedBuffers.end() && !found->second.consumed) {
                    violate(
                        FakeRule::StaleRelease,
                        "a buffer is released again before anything "
                        "acquired its earlier release"
                    );
                }
                parkedBuffers.insert_or_assign(
                    half.buffer,
                    Parked<RHIBufferBarrier>{.barrier = half}
                );
            }
        }

        Parked<RHITextureBarrier>* findParked(const RHITextureBarrier& half) {
            const auto found = parkedTextures.find(half.texture);
            if(found == parkedTextures.end())
                return nullptr;

            for(auto& entry: found->second) {
                if(entry.barrier == half)
                    return &entry;
            }

            return nullptr;
        }

        void checkAttachment(
            RHITexture* texture,
            RHILoadAction load,
            RHITextureLayout layout,
            std::span<const RHITextureBarrier> textureAcquires
        ) {
            const RHITextureBarrier* acquired = nullptr;
            for(const auto& half: textureAcquires) {
                if(half.texture == texture)
                    acquired = &half;
            }

            if(acquired == nullptr) {
                violate(
                    FakeRule::UnacquiredAttachment,
                    "an attachment has no acquire at its pass's begin"
                );

                return;
            }
            if(acquired->layoutAfter != layout) {
                violate(
                    FakeRule::UnacquiredAttachment,
                    "an attachment is acquired into another usage"
                );
            }
            if(acquired->discard && load == RHILoadAction::Load) {
                violate(
                    FakeRule::DiscardedLoad,
                    "an attachment loads what its acquire discards"
                );
            }
        }

        void checkBindings() {
            if(pushSet && viewSet)
                return;

            violate(
                FakeRule::FreshBindings,
                "a draw before its pass set a push and slot 0"
            );
        }
    };
}
