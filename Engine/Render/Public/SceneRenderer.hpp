#pragma once

#include <vector>

#include "Assert.hpp"
#include "GeometryPool.hpp"
#include "PipelineCache.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderMaterial.hpp"
#include "RenderSceneData.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RenderScene;

    // One ExecuteIndirectIndexed submission:
    // a contiguous run of the args buffer whose draws all share a pipeline.
    struct DrawBucket {
        RHIGraphicsPipelineState* pso = nullptr;
        u32 firstDraw = 0;
        u32 drawCount = 0;
    };

    // Where a surviving draw wants to land,
    // before the buckets know their offsets.
    struct VisibleDraw {
        GeometryAllocation geometry{};
        u32 bucket = 0;
        u32 primitive = 0;
        u32 materialIndex = 0;
    };

    using DrawArgs = std::vector<RHIDrawIndexedArgs>;
    using DrawBuckets = std::vector<DrawBucket>;
    using DrawRows = std::vector<DrawData>;
    using MaterialRows = std::vector<MaterialData>;
    using PipelineStatePtrs = std::vector<RHIGraphicsPipelineState*>;
    using ViewRecords = std::vector<ViewData>;
    using VisibleDraws = std::vector<VisibleDraw>;

    struct SceneRendererDesc {
        // worst cases, not live counts: the buffers cannot grow mid-frame
        u32 drawCapacity = 4096;
        u32 materialCapacity = 256;
        u32 viewCount = 1;
    };

    class SceneRenderer {
    private:
        // per-frame buffers, internally frame-indexed
        RHIBufferRAII drawDataBuffer;
        RHIBufferRAII argsBuffer;
        RHIBufferRAII materialBuffer;
        // one RHI_CB_ALIGN record per view, selected by offset
        RHIBufferRAII viewCB;

        PipelineCache pipelines;

        DrawRows drawScratch;
        DrawArgs argsScratch;
        MaterialRows materialScratch;
        ViewRecords views;
        // one resolved pipeline per material row, for the pass being built
        PipelineStatePtrs pipelineOfMaterial;
        VisibleDraws visibleScratch;
        DrawBuckets buckets;
        u32 drawCount = 0;
        u32 materialCount = 0;

        bool uploaded = false;

    public:
        ~SceneRenderer();
        CROWY_DECLARE_PINNED(SceneRenderer)

        SceneRenderer(RHIDevice& device, const SceneRendererDesc& desc);

        auto& View(this auto& self, u32 index) noexcept {
            CROWY_ASSERT(index < self.views.size());

            return self.views[index];
        }
        u32 ViewCount() const noexcept {
            return static_cast<u32>(views.size());
        }
        u32 DrawCapacity() const noexcept {
            return static_cast<u32>(drawScratch.size());
        }
        u32 DrawCount() const noexcept { return drawCount; }
        usize BucketCount() const noexcept { return buckets.size(); }
        usize PipelineCount() const noexcept { return pipelines.Count(); }

        // Culls against the given view,
        // then flattens the survivors into one row per submesh.
        // visibility changes every frame, so fully rebuild
        void BuildFrame(
            const RenderScene& scene,
            const PassPipelineDesc& pass,
            u32 viewIndex = 0
        );
        void Upload();

        ScenePush Push();

        void BindView(RHICommandList& cmdList, u32 slot, u32 viewIndex) const;
        // one ExecuteIndirectIndexed per bucket, each pointing into the same
        // args buffer by offset
        void Submit(
            RHICommandList& cmdList,
            const RHIIndexBufferView& indices
        ) const;

    private:
        // linear search, because a pass has a handful of buckets
        // and a hash map costs more than the scan
        u32 bucketOf(RHIGraphicsPipelineState* pso);
        void resolvePipelines(
            const RenderScene& scene,
            const PassPipelineDesc& pass
        );
    };
}
