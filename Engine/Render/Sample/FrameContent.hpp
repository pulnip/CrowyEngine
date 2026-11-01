#pragma once

#include <span>
#include <vector>

#include "FrameGraph.hpp"
#include "GeometryPool.hpp"
#include "InputProvider.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "Semantics.hpp"
#include "StandardFrame.hpp"

namespace Crowy
{
    class FrameContent {
    public:
        CROWY_DECLARE_INTERFACE(FrameContent)

        void BuildGeometry(GeometryPool& pool) { buildGeometry(pool); }

        void ExtractScene(RenderScene& scene) { extractScene(scene); }

        FrameGraphDesc DescribePipeline(const StandardFrameConfig& config) {
            return describePipeline(config);
        }

        void BindPass(RHICommandList& cmdList, const ScenePush& push) {
            bindPass(cmdList, push);
        }

        void UpdateFrameData() { updateFrameData(); }

        void PollInput(const InputProvider& input) {
            pollInput(input);
        }

        void UpdateScene(f64 deltaTime) { updateScene(deltaTime); }

        void InitUI(RHIDevice& device, const OverlayFormats& formats) {
            initUI(device, formats);
        }

        std::span<const RHITextureBarrier> PrepareUI(RHICommandList& cmdList) {
            return prepareUI(cmdList);
        }

        void RecordUI(RHICommandList& cmdList) { recordUI(cmdList); }

        usize ReloadShaders() { return reloadShaders(); }

        std::vector<PassHook> RecordSimulation(RHICommandList& cmdList) {
            return recordSimulation(cmdList);
        }

    protected:
        // Override to push a struct starting with the same members
        // when a shader wants more root constants. Runs in every mesh pass.
        virtual void bindPass(RHICommandList& cmdList, const ScenePush& push) {
            cmdList.SetPushGraphicsConstants(push);
        }

    private:
        // once, from Init; the pool queues the copies and the first frame
        // records them
        virtual void buildGeometry(GeometryPool& pool) = 0;

        // The only sample code allowed to see both where a thing is in the
        // world and what the renderer stores about it.
        virtual void extractScene(RenderScene& scene) = 0;

        // called again on every config change; an own desc clears scene color
        // with config.clearColor and ends with appendPostChain(config.post)
        virtual FrameGraphDesc describePipeline(
            const StandardFrameConfig& config
        ) {
            return describeStandardFrame(config);
        }

        // The sample's own per-frame buffers, written here
        // because the pass has not opened yet.
        virtual void updateFrameData() {}

        // input a sample reads beyond the camera's
        virtual void pollInput(const InputProvider&) {}

        // after the camera: the frame's time, or the fixed 1/60 step in a
        // counted run and a frame capture's run
        virtual void updateScene(f64) {}

        // the formats of the pass the UI rides, frozen for the app's life
        virtual void initUI(RHIDevice&, const OverlayFormats&) {}

        // runs before the first pass; the acquires ride the overlay pass
        virtual std::span<const RHITextureBarrier> prepareUI(RHICommandList&) {
            return {};
        }

        // runs inside the overlay pass, after its draws
        virtual void recordUI(RHICommandList&) {}

        // after the reload_shaders verb rebuilt the renderer's pipelines: the
        // sample's own programs; returns how many it rebuilt
        virtual usize reloadShaders() { return 0; }

        // runs before the first pass, after prepareUI: the sample's own
        // GPU work, and one binding per hook pass of the running list
        virtual std::vector<PassHook> recordSimulation(RHICommandList&) {
            return {};
        }
    };
}
