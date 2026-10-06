#pragma once

#include <map>
#include <optional>
#include <span>
#include <vector>

#include "FramePipeline.hpp"
#include "GeometryPool.hpp"
#include "MintFrame.hpp"
#include "PaintShared.h"
#include "PaintSurface.hpp"
#include "PaintWorld.hpp"
#include "PipelineCache.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderSceneData.hpp"
#include "RenderTexture.hpp"

namespace Crowy
{
    // A texture the sample owns across frames: the usage it rests in, and
    // the release this command list still owes an acquire. Pipeline targets
    // cannot persist from frame to frame, so the paint buffers live here.
    class PaintTexture {
    private:
        RHITextureRAII texture;
        RHIResourceUsage state = RHIResourceUsage::Undefined;
        std::optional<RHITextureBarrier> pending;
        bool touched = false;

    public:
        PaintTexture() = default;
        explicit PaintTexture(RHITextureRAII texture);

        RHITexture& Get() noexcept { return *texture; }
        explicit operator bool() const noexcept { return texture != nullptr; }
        RHITextureRAII Take() noexcept { return std::move(texture); }

        // a new command list: what it did not pair completes at its close
        void NewFrame() noexcept;
        // the acquire a pass using it as `use` begins with; none when it
        // already is in that use in this command list
        std::optional<RHITextureBarrier> Acquire(
            RHIResourceUsage use,
            bool discard = false
        );
        RHITextureBarrier Release(RHIResourceUsage next);
        // the release no pass of the sample's acquired, for a hook to
        const std::optional<RHITextureBarrier>& Pending() const noexcept {
            return pending;
        }
    };

    // mirrored from PaintSurface.slang's `surface` cbuffer: scalars and float4
    struct PaintSurfaceConstants {
        Mat4 objectToWorld = unitMat();
        Vec4 rotation[3]{};
        Vec4 scale3D{};
        Vec4 boundsMin{};
        Vec4 boundsSize{};
        Vec4 islands[PaintFaceDirectionCount]{};
        Vec4 atlas{};
        u32 paintMap = 0;
        u32 positionMap = 0;
        u32 edgeFadeMap = 0;
        u32 vbIndex = 0;
        u32 view = 0;
        u32 compareView = 0;
        u32 flags = 0;
        u32 selected = 0;
        Vec4 screen{};
        // DescriptorHandle<StructuredBuffer<uint>>: this surface's cell ids,
        // one per (voxel, direction), when a score view is shown
        u64 cellIds = 0;
        u32 cellBase = 0;
        u32 cellPad = 0;
        // xyz the grid's scaled-local origin, w its cell size
        Vec4 cellOrigin{};
        // xyz the voxel counts
        u32 cellDims[4]{};
    };
    static_assert(sizeof(PaintSurfaceConstants) % 16 == 0);

    // mirrored from PaintBrush.slang's BrushPush
    struct PaintBrushPush {
        u64 previous = 0;
        u64 position = 0;
        Vec4 boundsMin{};
        Vec4 boundsSize{};
        Vec4 centerRadius{};
        Vec4 axisUStretch{};
        Vec4 axisVImpact{};
        Vec4 splat{};
        Vec4 lockGens{};
        Vec4 shape{};
    };
    static_assert(sizeof(PaintBrushPush) <= RHI_PUSH_CONSTANT_BYTES);

    // mirrored from PanelAtlas.slang's PanelPush
    struct PaintPanelPush {
        u64 paint = 0;
        u64 position = 0;
        u64 edgeFade = 0;
        u32 channel = 0;
        u32 atlasSize = 0;
        Vec4 rect{};
        Vec4 islandRects[PaintFaceDirectionCount]{};
    };
    static_assert(sizeof(PaintPanelPush) <= RHI_PUSH_CONSTANT_BYTES);

    // what the surface pass needs besides the surfaces
    struct PaintDrawSettings {
        u32 view = PAINT_VIEW_LIT;
        u32 compareView = PAINT_VIEW_LIT;
        // the cell grids go up only for the views that show them
        bool showsCells = false;
        // the split's pixel column; past the right edge shows `view` alone
        f32 splitPixels = 1e9f;
    };

    // The surfaces' GPU half: one paint buffer, position atlas and edge fade
    // per surface, the brush that stamps splats into them, and the draws of
    // the paint and panel hooks.
    class PaintGpu {
    private:
        struct SurfaceGpu {
            PaintTexture paint;
            PaintTexture position;
            RHITextureRAII edgeFade;
            RHIBufferRAII staging;
            u32 atlasSize = 0;
            u64 builtVersion = 0;
            bool uploadPending = false;
            bool clearPending = false;
        };

        RHIDevice& device;
        std::vector<SurfaceGpu> surfaces;
        // one per atlas size, as MintChoco's GetScratchTarget
        std::map<u32, PaintTexture> scratch;
        // this frame's releases the hooks acquire; alive until Record ends
        std::vector<RHITextureBarrier> hookAcquires;
        // this frame's texture table and the rows each surface's maps name
        std::vector<TextureData> textureRows;
        RHIBufferSlice textureSlice;
        std::vector<u32> cellRows;

    public:
        explicit PaintGpu(RHIDevice& device);

        // a (re)laid-out surface gets new buffers, its atlas baked and queued
        // for upload; called before Record each frame
        void Sync(std::span<const PaintSurface> surfaces);
        // clears every buffer at the next Record
        void ClearAll();
        void Clear(usize surface);

        // uploads, clears and the frame's stamps in order, then the edges
        // the hooks acquire
        std::span<const RHITextureBarrier> Record(
            RHICommandList& cmdList,
            PipelineCache& pipelines,
            std::span<const PaintSurface> surfaces,
            std::span<const PaintStampDraw> stamps
        );

        // inside the paint hook: every active surface
        u32 DrawSurfaces(
            RHICommandList& cmdList,
            const HookPassContext& context,
            PipelineCache& pipelines,
            ScenePush push,
            RHIIndexBufferView indices,
            std::span<const PaintSurface> surfaces,
            std::span<const u8> active,
            std::span<const GeometryAllocation> geometry,
            const PaintDrawSettings& settings
        );

        // inside the panel hook: one surface's atlas in `rect` (pixels)
        u32 DrawPanel(
            RHICommandList& cmdList,
            const HookPassContext& context,
            PipelineCache& pipelines,
            const PaintSurface& surface,
            usize index,
            u32 channel,
            Vec4 rect
        );

        bool IsReady(usize surface) const noexcept;

    private:
        void rebuild(usize index, const PaintSurface& surface);
        void recordUploads(RHICommandList& cmdList);
        void recordClears(RHICommandList& cmdList);
        void recordStamp(
            RHICommandList& cmdList,
            PipelineCache& pipelines,
            const PaintStampDraw& stamp,
            const PaintSurface& surface,
            bool scratchUsedAgain
        );
        PaintTexture& scratchFor(u32 atlasSize);
        void collectHookAcquires();
    };
}
