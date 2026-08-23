#pragma once

#include <vector>

#include "Assert.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderMaterial.hpp"
#include "RenderSceneData.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using DrawArgs = std::vector<RHIDrawIndexedArgs>;
    using DrawRows = std::vector<DrawData>;
    using MaterialRows = std::vector<MaterialData>;
    using ViewRecords = std::vector<ViewData>;

    class RenderScene;

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

        DrawRows drawScratch;
        DrawArgs argsScratch;
        MaterialRows materialScratch;
        ViewRecords views;
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

        // Culls against the given view,
        // then flattens the survivors into one row per submesh.
        // visibility changes every frame, so fully rebuild
        void BuildFrame(const RenderScene& scene, u32 viewIndex = 0);
        void Upload();

        ScenePush Push();

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
