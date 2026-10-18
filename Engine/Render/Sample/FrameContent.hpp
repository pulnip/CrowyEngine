#pragma once

#include <span>
#include <vector>

#include "FramePipeline.hpp"
#include "GeometryPool.hpp"
#include "InputProvider.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "Semantics.hpp"
#include "StandardPipeline.hpp"

namespace Crowy
{
    class FrameHost;

    // What a scene adds to the frame a FrameHost runs: its geometry and rows,
    // its pass list and per-pass work, and its UI.
    class FrameContent {
    private:
        friend class FrameHost;

    public:
        CROWY_DECLARE_INTERFACE(FrameContent)

    protected:
        // once, from OnInit; the pool queues the copies and the first frame
        // records them
        virtual void OnBuildGeometry(GeometryPool& pool) = 0;

        // The only sample code allowed to see both where a thing is in the
        // world and what the renderer stores about it.
        virtual void ExtractScene(RenderScene& scene) = 0;

        // The pass list; the default is the standard one. Called at init
        // and again whenever the config changes, as a debug view does. An
        // override that builds its own desc clears scene colour with
        // config.clearColor and ends with appendPostChain(config.post).
        virtual FramePipelineDesc DescribePipeline(
            const StandardPipelineConfig& config
        ) {
            return makeStandardPipeline(config);
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

        // after the camera, on every frame the loop runs: the frame's time,
        // the fixed 1/60 step on a frame a counted run lets through, and on
        // every frame of a frame capture's run
        virtual void OnUpdateScene(f64) {}

        // the formats of the pass the UI rides, frozen for the app's life
        virtual void OnInitUI(RHIDevice&, const OverlayFormats&) {
            // default no-op so a sample without UI is unchanged
        }

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
