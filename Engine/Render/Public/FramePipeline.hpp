#pragma once

#include <optional>
#include <span>
#include <variant>
#include <vector>

#include "DrawList.hpp"
#include "Function.hpp"
#include "PipelineCache.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderSceneData.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RenderScene;
    class SceneRenderer;
    struct PassDesc;
    struct FixedSize;
    struct FrameExtentDesc;
    struct FrameTargetDesc;

    using FrameTargetID = u32;
    using FrameExtentID = u32;
    // the extent a target follows, or a size of its own that only a new desc
    // changes
    using FrameTargetSize = std::variant<FrameExtentID, FixedSize>;
    using FrameExtentDescs = std::vector<FrameExtentDesc>;
    using FrameTargetDescs = std::vector<FrameTargetDesc>;
    using PassDescs = std::vector<PassDesc>;

    // imported each frame: the swapchain image App::Render hands OnRecord
    inline constexpr FrameTargetID BackBufferTarget = 0;
    // the swapchain's size, following Resize
    inline constexpr FrameExtentID BackBufferExtent = 0;

    struct FixedSize {
        u32 width = 0;
        u32 height = 0;

        friend bool operator==(const FixedSize&, const FixedSize&) = default;
    };

    // extents[i] is ID i + 1: a size the host sets with ResizeExtent, which
    // starts at the back buffer's
    struct FrameExtentDesc {
        Str name;
    };

    // targets[i] is ID i + 1; usage bits come from the uses
    struct FrameTargetDesc {
        Str name;
        RHIPixelFormat format = RHIPixelFormat::Unknown;
        FrameTargetSize size = BackBufferExtent;
        Color clearColor = Colors::Black;
        f32 clearDepth = 1.0f;
    };

    struct ColorTargetUse {
        FrameTargetID target = BackBufferTarget;
        RHILoadAction load = RHILoadAction::Clear;
        RHIStoreAction store = RHIStoreAction::Store;
    };

    struct DepthTargetUse {
        FrameTargetID target = BackBufferTarget;
        RHILoadAction load = RHILoadAction::Clear;
        RHIStoreAction store = RHIStoreAction::Store;
    };

    struct MeshPassDesc {
        // the ViewData row
        u32 view = 0;
        // domains and required PrimitiveFlags
        DrawFilter filter;
        DrawOrder order = DrawOrder::PipelineThenNearFirst;
        MeshPassState state;
        // the list's reserve; its scratch grows
        u32 drawCapacity = 256;
    };

    // one triangle; the push is the reads' IDs plus params
    struct FullscreenPassDesc {
        RHIShaderDesc fragmentShader;
        // Load and blend onto a target the pass does not read
        std::optional<RHIBlendState> blend;
        Vec4 params{};
    };

    // draws a sample records: FrameInputs::hooks binds a record to the
    // name, which the walker calls after opening the pass and binding view 0
    struct HookPassDesc {
        Str hook;
    };

    // no draws of its own: the UI is its content, sampling its reads by
    // OverlayReadableID
    struct OverlayPassDesc {};

    struct PassDesc {
        // the event, the encoder label, the per-pass stats
        Str name;
        // empty: depth-only
        std::vector<ColorTargetUse> colors;
        std::optional<DepthTargetUse> depth;
        // sampled in the fragment stage
        std::vector<FrameTargetID> reads;
        std::variant<
            MeshPassDesc,
            FullscreenPassDesc,
            HookPassDesc,
            OverlayPassDesc>
            kind;
    };

    struct FramePipelineDesc {
        FrameTargetDescs targets;
        PassDescs passes;
        // the target whose clear a debug view edits
        FrameTargetID sceneColor = BackBufferTarget;
        // a fixed-size square depth target the directional shadow is
        // rendered into; 0 for none
        FrameTargetID shadowMap = 0;
        FrameExtentDescs extents;
    };

    // the color and depth format of the pass the UI rides
    struct OverlayFormats {
        RHIPixelFormat color = RHIPixelFormat::Unknown;
        RHIPixelFormat depth = RHIPixelFormat::Unknown;

        friend bool operator==(const OverlayFormats&, const OverlayFormats&) =
            default;
    };

    // the formats of a hook pass's attachments
    struct HookPassFormats {
        std::span<const RHIPixelFormat> colors;
        RHIPixelFormat depth = RHIPixelFormat::Unknown;
    };

    // what a hook's record sees inside its pass
    struct HookPassContext {
        StrView pass;
        HookPassFormats formats;
        // the readable IDs of the pass's reads, in order
        std::span<const u64> reads;
    };

    // a sample's draws for one hook pass, and what its own work released
    // for them, acquired at the pass's begin
    struct PassHook {
        Str name;
        std::span<const RHIBufferBarrier> bufferAcquires;
        std::span<const RHITextureBarrier> textureAcquires;
        // returns the draws it recorded
        std::move_only_function<u32(RHICommandList&, const HookPassContext&)>
            record;
    };

    struct PassStats {
        Str name;
        // the list's, or a hook pass's record's; 0 for a fullscreen pass
        u32 draws = 0;
        u32 runs = 0;
        u64 triangles = 0;
        // the acquires and releases this pass's begin and end carried
        u32 barrierEdges = 0;
    };

    // a named target of `frame`, to be written to `path` as a BMP
    struct TargetCaptureRequest {
        u64 frame = 0;
        Str target;
        Str path;
    };

    // one copy Record made, waiting for its frame to complete
    struct TargetReadback {
        TargetCaptureRequest request;
        // resolved from the name by the caller; Record fills the rest
        FrameTargetID target = BackBufferTarget;
        // the frame whose submission carries the copy
        u64 recorded = 0;
        RHIBufferRAII buffer;
        RHIPixelFormat format = RHIPixelFormat::Unknown;
        u32 width = 0;
        u32 height = 0;
        u32 rowPitch = 0;
    };

    // what one Record needs and does not own; RenderApp fills it per frame
    struct FrameInputs {
        // only the texture: the desc says how each pass loads it
        RHITexture* backBuffer = nullptr;
        Color sceneClear = Colors::Black;
        RHIIndexBufferView indices{};
        // ScenePush::vertices
        u64 vertices = 0;
        std::span<const RHIBufferBarrier> geometryAcquires;
        // sets a mesh pass's push; unset, the walker pushes it as it is
        std::move_only_function<void(RHICommandList&, const ScenePush&)>
            bindMeshPass;
        // OnPrepareUI's, on the overlay pass's begin
        std::span<const RHITextureBarrier> overlayAcquires;
        // inside the overlay pass, after its draws
        std::move_only_function<void(RHICommandList&)> recordOverlay;
        // one per hook pass in the list, and no other
        std::span<PassHook> hooks;
        // the frame Record runs in, as Submit numbers it
        u64 frame = 0;
        // one per target, each copied out after its last use in a blit pass
        // that ends the frame
        std::span<TargetReadback> captures;
    };

    // Walks a pass list in order; every barrier between two passes is
    // compiled from what each pass declares it attaches and reads.
    class FramePipeline {
    private:
        // one half of an edge; the texture resolves at Record, because the
        // back buffer changes every frame and a target on Resize
        struct CompiledBarrier {
            FrameTargetID target = BackBufferTarget;
            RHIResourceUsage before = RHIResourceUsage::Undefined;
            RHIResourceUsage after = RHIResourceUsage::Undefined;
            // the first use, against the frame before's last one
            bool crossSubmission = false;
        };

        using CompiledBarriers = std::vector<CompiledBarrier>;
        using PixelFormats = std::vector<RHIPixelFormat>;

        struct CompiledPass {
            PixelFormats colorFormats;
            RHIPixelFormat depthFormat = RHIPixelFormat::Unknown;
            CompiledBarriers acquires;
            CompiledBarriers releases;
            // a mesh pass's; null for the others
            RAII<DrawList> drawList;
            // a fullscreen pass's key, resolved in every Prepare, so a
            // shader reload reaches it
            std::optional<RHIGraphicsPipelineStateDesc> fullscreenDesc;
            RHIGraphicsPipelineState* fullscreenPipeline = nullptr;
        };

        // a target's last pass and what it does there, which a capture
        // follows
        struct LastUse {
            usize pass = 0;
            RHIResourceUsage usage = RHIResourceUsage::Undefined;
        };

        using CompiledPasses = std::vector<CompiledPass>;
        using ExtentSizes = std::vector<Size2D>;
        using LastUses = std::vector<LastUse>;
        using PassStatsList = std::vector<PassStats>;
        using TargetTextures = std::vector<RHITextureRAII>;
        using TargetUsages = std::vector<RHITextureUsage>;
        using TextureBarriers = std::vector<RHITextureBarrier>;
        using ColorAttachments = std::vector<RHIColorAttachment>;
        using HookBindings = std::vector<PassHook*>;
        using ReadIDs = std::vector<u64>;

    private:
        RHIDevice& device;
        FramePipelineDesc desc;
        RHIPixelFormat backBufferFormat = RHIPixelFormat::Unknown;
        // indexed by FrameExtentID
        ExtentSizes extentSizes;

        // indexed by FrameTargetID; the back buffer's and an unused
        // target's stay null
        TargetTextures textures;
        // the union of each target's uses; None for one no pass uses
        TargetUsages usages;
        // indexed by FrameTargetID; the back buffer's and an unused
        // target's are never read
        LastUses lastUses;
        CompiledPasses passes;
        PassStatsList stats;
        // the last pass writing the back buffer: the UI rides it
        usize overlayPass = 0;
        // the largest mesh pass view + 1
        u32 viewCount = 1;

        TextureBarriers acquireScratch;
        TextureBarriers releaseScratch;
        ColorAttachments colorScratch;
        // per pass, the frame's binding of a hook pass; null for the others
        HookBindings hookBindings;
        ReadIDs readScratch;

    public:
        ~FramePipeline();
        CROWY_DECLARE_PINNED(FramePipeline)

        // throws std::invalid_argument naming the pass and the rule a desc
        // breaks
        FramePipeline(
            RHIDevice& device,
            FramePipelineDesc desc,
            RHIPixelFormat backBufferFormat,
            u32 width,
            u32 height
        );

        // the swapchain-sized targets anew; the old ones retire
        void Resize(u32 width, u32 height);
        // the targets of `extent` anew at that size, when it changed; the old
        // ones retire
        void ResizeExtent(FrameExtentID extent, u32 width, u32 height);
        // per mesh pass: its view's cull, then its list built and uploaded;
        // per fullscreen pass: its pipeline resolved
        void Prepare(
            SceneRenderer& renderer,
            const RenderScene& scene,
            const MeshPassOverride& debug
        );
        // throws std::invalid_argument, before any pass, when inputs.hooks
        // does not bind each hook pass exactly once and nothing else
        void Record(
            RHICommandList& cmdList,
            const SceneRenderer& renderer,
            FrameInputs& inputs
        );

        OverlayFormats Overlay() const noexcept;
        // the formats of the pass naming `hook`; empty when no pass does, as
        // in a data view's list
        std::optional<HookPassFormats> FindHook(StrView hook) const noexcept;
        std::span<const PassStats> Stats() const noexcept { return stats; }
        // the ViewData rows the mesh passes name, so SceneRenderer keeps them
        u32 ViewCount() const noexcept { return viewCount; }
        // the width of desc.shadowMap's target; 0 without one
        u32 ShadowMapSize() const noexcept;
        // the target of that name, if some pass uses it
        std::optional<FrameTargetID> FindTarget(StrView name) const noexcept;
        // targets[i] is ID i + 1
        std::span<const FrameTargetDesc> Targets() const noexcept {
            return desc.targets;
        }
        std::optional<FrameExtentID> FindExtent(StrView name) const noexcept;
        Size2D TargetSize(FrameTargetID id) const noexcept;
        FrameTargetID SceneColor() const noexcept { return desc.sceneColor; }
        // a target the overlay pass reads, for the UI to sample; it changes
        // when its extent resizes
        u64 OverlayReadableID(FrameTargetID id) const;

    private:
        // every target when `only` is empty, else the targets that follow it
        void createTargets(std::optional<FrameExtentID> only);
        RHITexture& texture(FrameTargetID id, const FrameInputs& inputs) const;
        RHITextureBarrier makeBarrier(
            const CompiledBarrier& half,
            const FrameInputs& inputs
        ) const;
        // whether pass `pass` holds the last use of a target captured this
        // frame
        bool capturedAfter(
            usize pass,
            FrameTargetID id,
            const FrameInputs& inputs
        ) const noexcept;
        void recordCaptures(RHICommandList& cmdList, FrameInputs& inputs);
        // each hook pass's binding in inputs.hooks, or a refusal
        void bindHooks(FrameInputs& inputs);
    };
}
