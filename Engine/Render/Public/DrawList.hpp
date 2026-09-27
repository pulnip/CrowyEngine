#pragma once

#include <bit>
#include <utility>
#include <vector>

#include "GeometryPool.hpp"
#include "PipelineCache.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderMaterial.hpp"
#include "RenderScene.hpp"
#include "RenderSceneData.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct DrawRun;
    struct VisibleDraw;

    using DrawArgs = std::vector<RHIDrawIndexedArgs>;
    using DrawRows = std::vector<DrawData>;
    using DrawRuns = std::vector<DrawRun>;
    using PipelineStatePtrs = std::vector<RHIGraphicsPipelineState*>;
    // (key, visible index): the index keeps submesh order and makes the
    // order total
    using SortedDraws = std::vector<std::pair<u64, u32>>;
    using VisibleDraws = std::vector<VisibleDraw>;

    // unsigned order of the result is the float order of the argument
    inline constexpr u32 ordered(f32 value) noexcept {
        const auto bits = std::bit_cast<u32>(value);

        return (bits & 0x8000'0000u) != 0 ? ~bits : bits | 0x8000'0000u;
    }

    // pipeline first, then near first
    inline constexpr u64 opaqueSortKey(
        u32 run,
        f32 depth,
        u32 identity
    ) noexcept {
        return u64{run} << 48 | u64{ordered(depth)} << 16 | (identity & 0xFFFFu);
    }

    // far first, whatever the pipeline
    inline constexpr u64 translucentSortKey(f32 depth, u32 identity) noexcept {
        return u64{~ordered(depth)} << 32 | identity;
    }

    enum class DrawOrder : u8 {
        PipelineThenNearFirst,
        FarFirst,
    };

    struct DrawFilter {
        // a mask of MaterialDomain bits
        MaterialDomain domains = MaterialDomain::Opaque;
        // every one of these set on the primitive
        PrimitiveFlags required = PrimitiveFlags::None;
    };

    // one submesh a view's cull kept
    struct VisibleDraw {
        GeometryAllocation geometry{};
        // the primitive's row: DrawData::objectID
        u32 primitive = 0;
        // the primitive's handle slot, which PackedTable::Remove never moves
        u32 identity = 0;
        u32 materialIndex = 0;
        // the bounds center's clip z before the divide
        f32 depth = 0.0f;
        PrimitiveFlags flags = PrimitiveFlags::None;
    };

    struct VisibleSet {
        VisibleDraws draws;
        u32 primitiveCount = 0;
    };

    // one ExecuteIndirectIndexed: a maximal span sharing a pipeline
    struct DrawRun {
        RHIGraphicsPipelineState* pso = nullptr;
        u32 firstDraw = 0;
        u32 drawCount = 0;
    };

    // One pass's draws, in the list's own transient slices.
    class DrawList {
    private:
        RHIDevice& device;

        // this frame's slices, refreshed by Upload()
        RHIBufferSlice rowSlice;
        RHIBufferSlice argsSlice;

        DrawRows rows;
        DrawArgs args;
        DrawRuns runs;
        SortedDraws sorted;
        // one resolved pipeline per material row, null outside the filter
        PipelineStatePtrs pipelineOfMaterial;
        // the distinct pipelines in material-row order; a run's index here
        // is its sort key's most significant part
        PipelineStatePtrs pipelineOrder;
        std::vector<u32> runOfMaterial;
        u64 triangleCount = 0;

        bool uploaded = false;

    public:
        ~DrawList();
        CROWY_DECLARE_PINNED(DrawList)

        DrawList(RHIDevice& device, u32 drawReserve);

        // resolves every material the filter admits, visible or not, so
        // pipelines compile on the first frame; then keys, sorts and forms runs
        void Build(
            const RenderScene& scene,
            const VisibleSet& visible,
            PipelineCache& cache,
            const PassPipelineDesc& pass,
            const DrawFilter& filter,
            DrawOrder order
        );
        // fresh slices each call; an empty list allocates nothing
        void Upload();
        // the frame's push with this list's rows
        ScenePush Push(ScenePush frame) const;
        // one ExecuteIndirectIndexed per run
        void Submit(
            RHICommandList& cmdList,
            const RHIIndexBufferView& indices
        ) const;

        u32 DrawCount() const noexcept { return static_cast<u32>(rows.size()); }
        usize RunCount() const noexcept { return runs.size(); }
        u64 TriangleCount() const noexcept { return triangleCount; }
    };
}
