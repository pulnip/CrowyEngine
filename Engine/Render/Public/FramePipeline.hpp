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
    struct FrameTargetDesc;

    using FrameTargetID = u32;
    using FrameTargetDescs = std::vector<FrameTargetDesc>;
    using PassDescs = std::vector<PassDesc>;

    // imported each frame: the swapchain image App::Render hands OnRecord
    inline constexpr FrameTargetID BackBufferTarget = 0;

    // targets[i] is ID i + 1; usage bits come from the uses
    struct FrameTargetDesc {
        Str name;
        RHIPixelFormat format = RHIPixelFormat::Unknown;
        // 0: the swapchain's, following Resize
        u32 width = 0;
        u32 height = 0;
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

    struct PassDesc {
        // the event, the encoder label, the per-pass stats
        Str name;
        // empty: depth-only
        std::vector<ColorTargetUse> colors;
        std::optional<DepthTargetUse> depth;
        // sampled in the fragment stage
        std::vector<FrameTargetID> reads;
        std::variant<MeshPassDesc, FullscreenPassDesc> kind;
    };

    struct FramePipelineDesc {
        FrameTargetDescs targets;
        PassDescs passes;
        // the target whose clear a debug view edits
        FrameTargetID sceneColor = BackBufferTarget;
        // a fixed-size square depth target the directional shadow is
        // rendered into; 0 for none
        FrameTargetID shadowMap = 0;
    };

    // the color and depth format of the pass the UI rides
    struct OverlayFormats {
        RHIPixelFormat color = RHIPixelFormat::Unknown;
        RHIPixelFormat depth = RHIPixelFormat::Unknown;

        friend bool operator==(const OverlayFormats&, const OverlayFormats&) =
            default;
    };

    struct PassStats {
        Str name;
        // the list's; 0 for a fullscreen pass
        u32 draws = 0;
        u32 runs = 0;
        u64 triangles = 0;
        // the acquires and releases this pass's begin and end carried
        u32 barrierEdges = 0;
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
            // a mesh pass's; null for a fullscreen pass
            RAII<DrawList> drawList;
            // a fullscreen pass's key, resolved in every Prepare, so a
            // shader reload reaches it
            std::optional<RHIGraphicsPipelineStateDesc> fullscreenDesc;
            RHIGraphicsPipelineState* fullscreenPipeline = nullptr;
        };

        using CompiledPasses = std::vector<CompiledPass>;
        using PassStatsList = std::vector<PassStats>;
        using TargetTextures = std::vector<RHITextureRAII>;
        using TargetUsages = std::vector<RHITextureUsage>;
        using TextureBarriers = std::vector<RHITextureBarrier>;
        using ColorAttachments = std::vector<RHIColorAttachment>;

    private:
        RHIDevice& device;
        FramePipelineDesc desc;
        RHIPixelFormat backBufferFormat = RHIPixelFormat::Unknown;
        u32 width = 0;
        u32 height = 0;

        // indexed by FrameTargetID; the back buffer's and an unused
        // target's stay null
        TargetTextures textures;
        // the union of each target's uses; None for one no pass uses
        TargetUsages usages;
        CompiledPasses passes;
        PassStatsList stats;
        // the last pass writing the back buffer: the UI rides it
        usize overlayPass = 0;
        // the largest mesh pass view + 1
        u32 viewCount = 1;

        TextureBarriers acquireScratch;
        TextureBarriers releaseScratch;
        ColorAttachments colorScratch;

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
        // per mesh pass: its view's cull, then its list built and uploaded;
        // per fullscreen pass: its pipeline resolved
        void Prepare(
            SceneRenderer& renderer,
            const RenderScene& scene,
            const MeshPassOverride& debug
        );
        void Record(
            RHICommandList& cmdList,
            const SceneRenderer& renderer,
            FrameInputs& inputs
        );

        OverlayFormats Overlay() const noexcept;
        std::span<const PassStats> Stats() const noexcept { return stats; }
        // the ViewData rows the mesh passes name, so SceneRenderer keeps them
        u32 ViewCount() const noexcept { return viewCount; }
        // the width of desc.shadowMap's target; 0 without one
        u32 ShadowMapSize() const noexcept;

    private:
        void createTargets(bool swapchainSizedOnly);
        RHITexture& texture(FrameTargetID id, const FrameInputs& inputs) const;
        RHITextureBarrier makeBarrier(
            const CompiledBarrier& half,
            const FrameInputs& inputs
        ) const;
    };
}
