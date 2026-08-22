#pragma once

#include <vector>

#include "Assert.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderSceneData.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using DrawArgs = std::vector<RHIDrawIndexedArgs>;
    using DrawRows = std::vector<DrawData>;
    using ViewRecords = std::vector<ViewData>;

    class RenderScene;

    class SceneRenderer {
    private:
        // per-frame buffers, internally frame-indexed
        RHIBufferRAII drawDataBuffer;
        RHIBufferRAII argsBuffer;
        // one RHI_CB_ALIGN record per view, selected by offset
        RHIBufferRAII viewCB;

        DrawRows drawScratch;
        DrawArgs argsScratch;
        ViewRecords views;
        u32 drawCount = 0;

        bool uploaded = false;

    public:
        ~SceneRenderer();
        CROWY_DECLARE_PINNED(SceneRenderer)

        SceneRenderer(RHIDevice& device, u32 drawCapacity, u32 viewCount = 1);

        auto& View(this auto& self, u32 index) noexcept {
            CROWY_ASSERT(index < self.views.size());

            return self.views[index];
        }

        auto ViewCount() const noexcept {
            return views.size();
        }
        auto DrawCapacity() const noexcept {
            return drawScratch.size();
        }
        u32 DrawCount() const noexcept { return drawCount; }

        // Culls against the given view, then flattens the survivors.
        // visibility changes every frame, so fully rebuild
        void BuildFrame(const RenderScene& scene, u32 viewIndex = 0);
        // Writing one after a pass opens is
        // either read by the wrong draws or illegal outright.
        void Upload();

        u64 DrawDataID();

        void BindView(RHICommandList& cmdList, u32 slot, u32 viewIndex) const;
        // one ExecuteIndirectIndexed over the whole visible list;
        // PSO bucketing splits this into one call per bucket via argsOffset
        void Submit(
            RHICommandList& cmdList,
            RHIGraphicsPipelineState& pso,
            const RHIIndexBufferView& indices
        ) const;
    };
}
