#pragma once

#include <vector>

#include "Assert.hpp"
#include "DrawList.hpp"
#include "PipelineCache.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderMaterial.hpp"
#include "RenderSceneData.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RenderScene;

    using MaterialRows = std::vector<MaterialData>;
    using ViewRecords = std::vector<ViewData>;

    struct SceneRendererDesc {
        // a reserve: the rows are transient, so the scratch grows
        u32 materialCapacity = 256;
        u32 viewCount = 1;
    };

    // What every pass of a frame shares, and one cull per view.
    class SceneRenderer {
    private:
        // a view's cull, kept until the next BeginFrame
        struct ViewCull {
            VisibleSet visible;
            bool culled = false;
        };

        using ViewCulls = std::vector<ViewCull>;

    private:
        RHIDevice& device;

        // this frame's transient slices, refreshed by Upload()
        RHIBufferSlice materialSlice;
        // one RHI_CB_ALIGN record per view, selected by offset
        RHIBufferSlice viewSlice;

        PipelineCache pipelines;

        MaterialRows materialScratch;
        ViewRecords views;
        ViewCulls culls;
        // what BeginFrame was given, and what Visible culls
        const RenderScene* frameScene = nullptr;

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
        usize PipelineCount() const noexcept { return pipelines.Count(); }
        // every cached pipeline, recompiled from disk (PipelineCache::Rebuild)
        PipelineRebuild ReloadPipelines() { return pipelines.Rebuild(); }
        // the cache every draw list resolves through
        auto& Pipelines(this auto& self) noexcept { return self.pipelines; }

        // every material row once for every list; forgets last frame's culls
        void BeginFrame(const RenderScene& scene);
        // culled on the view's first request of the frame
        const VisibleSet& Visible(u32 viewIndex);
        // the materials and the views
        void Upload();
        // the materials; a list adds its rows, the caller the vertices
        ScenePush FramePush() const;

        void BindView(RHICommandList& cmdList, u32 slot, u32 viewIndex) const;
    };
}
