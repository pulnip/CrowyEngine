#pragma once

#include <span>
#include <vector>

#include "AssetRoots.hpp"
#include "FrameGraph.hpp"
#include "GeometryPool.hpp"
#include "InputProvider.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "RenderSession.hpp"
#include "Semantics.hpp"
#include "StandardFrame.hpp"

namespace Crowy
{
    class FrameHost;

    class FrameContent {
    private:
        friend class FrameHost;

    public:
        CROWY_DECLARE_INTERFACE(FrameContent)

    protected:
        // once, from OnInit, with the device and the pool made and before
        // OnBuildGeometry: the session the host draws. The caller owns it and
        // destroys it before the host does.
        virtual RenderSession& OnOpenSession(
            RHIDevice& device,
            GeometryPool& pool,
            const AssetRoots& roots
        ) = 0;

        // once, from OnInit; the pool queues the copies and the first frame
        // records them
        virtual void OnBuildGeometry(GeometryPool& pool) = 0;

        // The only sample code allowed to see both where a thing is in the
        // world and what the renderer stores about it.
        virtual void ExtractScene(RenderScene& scene) = 0;

        // called again on every config change; an own desc clears scene color
        // with config.clearColor and ends with appendPostChain(config.post)
        virtual FrameGraphDesc DescribePipeline(
            const StandardFrameConfig& config
        ) {
            return describeStandardFrame(config);
        }

        // Override to push a struct starting with the same members
        // when a shader wants more root constants. Runs in every mesh pass.
        virtual void OnBindPass(
            RHICommandList& cmdList,
            const ScenePush& push
        ) {
            cmdList.SetPushGraphicsConstants(push);
        }

        // The sample's own per-frame buffers, written here
        // because the pass has not opened yet.
        virtual void OnUpdateFrameData() {}

        // input a sample reads beyond the camera's
        virtual void OnProcessInput(const InputProvider&) {}

        // after the camera: the frame's time, or the fixed 1/60 step in a
        // counted run and a frame capture's run
        virtual void OnUpdateScene(f64) {}

        // the formats of the pass the UI rides, frozen for the app's life
        virtual void OnInitUI(RHIDevice&, const OverlayFormats&) {}

        // runs before the first pass; the acquires ride the overlay pass
        virtual std::span<const RHITextureBarrier> OnPrepareUI(
            RHICommandList&
        ) {
            return {};
        }

        // runs inside the overlay pass, after its draws
        virtual void OnRecordUI(RHICommandList&) {}

        // after the reload_shaders verb rebuilt the renderer's pipelines: the
        // sample's own programs; returns how many it rebuilt
        virtual usize OnReloadShaders() { return 0; }

        // runs before the first pass, after OnPrepareUI: the sample's own
        // GPU work, and one binding per hook pass of the running list
        virtual std::vector<PassHook> OnRecordSimulation(RHICommandList&) {
            return {};
        }
    };
}
