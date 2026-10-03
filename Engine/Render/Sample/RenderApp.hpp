#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "AppFramework.hpp"
#include "Camera.hpp"
#include "CommandPort.hpp"
#include "EnumUtil.hpp"
#include "FrameHistory.hpp"
#include "FramePipeline.hpp"
#include "FrameProfiler.hpp"
#include "GeometryPool.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"
#include "StandardPipeline.hpp"
#include "TargetCapture.hpp"

namespace Crowy
{
    using FramePipelinePtr = RAII<FramePipeline>;
    using GeometryPoolPtr = RAII<GeometryPool>;
    using SceneRendererPtr = RAII<SceneRenderer>;
    using CommandPortPtr = RAII<CommandPort>;
    using TargetCaptureQueuePtr = RAII<TargetCaptureQueue>;

    // mirrored by the constants in Engine/Shader/DebugView.slang
    enum class DebugMode : u32 {
        Lit,
        Unshaded,
        Normals,
        Depth,
        Overdraw,
        // the lights' shadow term: 1 white, 0 black
        Shadow,
    };

    CROWY_ENUM_BEGIN(DebugMode)
        CROWY_ENUM_VALUE(Lit)
        CROWY_ENUM_VALUE(Unshaded)
        CROWY_ENUM_VALUE(Normals)
        CROWY_ENUM_VALUE(Depth)
        CROWY_ENUM_VALUE(Overdraw)
        CROWY_ENUM_VALUE(Shadow)
    CROWY_ENUM_END()

    // what the frame shows instead of the plain lit picture, exposed as `debug`
    struct RenderDebug {
        DebugMode mode = DebugMode::Lit;
        bool wireframe = false;
        // off, the opaque round tests Less and writes depth itself; a view
        // that overrides fill mode or depth draws without a prepass anyway
        bool depthPrepass = true;
        // Hard is the exact 0 or 1 every golden is recorded with
        ShadowFilter shadowFilter = ShadowFilter::Hard;
        // read by a sample that hosts the stats overlay or a panel
        bool showStats = false;
        bool showPanel = false;

        friend bool operator==(
            const RenderDebug&,
            const RenderDebug&
        ) = default;
    };

    // Sample framework for Engine/Render: Owns the frame order and seals it
    class RenderApp: public App {
    public:
        // the sample's statement of what its capture is taken against
        struct Config {
            RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
            // what the colour passes write; RGBA8_UNORM for a scene already
            // in display values
            RHIPixelFormat sceneColorFormat = RHIPixelFormat::RGBA16_FLOAT;
            // the lit views' scene colour; the data views clear black
            Color clearColor = Colors::Black;
            // the lit views' post list; the data views copy through Present
            std::vector<PostPassDesc> post{tonemapPass()};

            // reserves: the scratch grows, and the transient ring is the limit
            u32 drawCapacity = 4096;
            u32 materialCapacity = 256;
            // the standard pipeline's shadow map; 0 for a sample with no
            // lights, which keeps one view row and no Shadow pass
            u32 shadowMapSize = 2048;

            // element counts, as GeometryPool takes them
            u32 vertexPoolCapacity = 1024;
            u32 indexPoolCapacity = 4096;
        };

        // one finished frame: what the loop measured and what the renderer
        // counted, taken together so every reader sees the same frame
        struct FrameStats {
            FrameReport report;
            usize primitives = 0;
            u32 visiblePrimitives = 0;
            u64 triangles = 0;
            u32 draws = 0;
            usize runs = 0;
            usize pipelines = 0;
            // in list order; draws, runs and triangles sum these
            std::vector<PassStats> passes;
        };

        static constexpr u32 ViewMain = 0;

    private:
        // how far back read_stats can name a frame
        static constexpr usize FrameStatsDepth = 64;
        using FrameStatsHistory = FrameHistory<FrameStats, FrameStatsDepth>;

        // a wait_frame reply, held until its frame has ended
        struct PendingWait {
            u64 frame = 0;
            Reply reply;
        };

        Config config;

        RHIDevice* device = nullptr;
        RHISwapchain* swapchain = nullptr;
        f32 aspect = 1.0f;

        GeometryPoolPtr geometryPool;
        SceneRendererPtr renderer;
        // what DescribePipeline was last asked for, from config and the debug
        // view; a change rebuilds it
        StandardPipelineConfig pipelineConfig;
        FramePipelinePtr pipeline;
        // the hooks are bound once; the rest is refilled every frame
        FrameInputs frameInputs;
        RenderScene scene;
        CameraRAII camera;
        // declared before the port, which points at it, so it outlives it
        RenderDebug debug;

        // debug builds only; null when disabled, inert with
        // Status().server == BindFailed when no port could bind
        CommandPortPtr port;
        // a wait the port already timed out stays until its frame ends, and
        // answering it then does nothing
        std::vector<PendingWait> pendingWaits;
        FrameStatsHistory frameStats;
        // captures whose dump failed, since launch
        u32 captureFailures = 0;
        // the named targets' captures; the swapchain keeps the back buffer's
        TargetCaptureQueuePtr targetCaptures;
        // this frame's, filled by the walker and then handed to the queue
        TargetReadbacks frameCaptures;
        // OnRecordSimulation's, alive until the walker has recorded
        std::vector<PassHook> frameHooks;

        // this frame's counts, summed over its passes; OnFrameEnd adds the
        // report and the totals
        FrameStats recorded;

        bool reportedCullStats = false;

    public:
        ~RenderApp() override;
        CROWY_DECLARE_PINNED(RenderApp)

        RenderApp(const Config& config, CameraRAII camera);

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override final;
        void NewFrame() override final;
        void ProcessInput(const InputProvider& input) override final;
        void OnUpdate(f64 deltaTime, f64 elapsedTime) override final;
        void OnRecord(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override final;
        void OnFrameEnd(const FrameReport& report) override final;
        void OnResize(u32 width, u32 height) override final;

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
        );

        // Override to push a struct starting with the same members
        // when a shader wants more root constants. Runs in every mesh pass.
        virtual void OnBindPass(RHICommandList& cmdList, const ScenePush& push);

        // The sample's own per-frame buffers, written here
        // because the pass has not opened yet.
        virtual void OnUpdateFrameData() {}

        // input a sample reads beyond the camera's
        virtual void OnProcessInput(const InputProvider&) {}

        // after the camera, on every frame the loop runs: the frame's time,
        // the fixed 1/60 step on a frame a counted run lets through
        virtual void OnUpdateScene(f64) {}

        // the formats of the pass the UI rides, frozen for the app's life
        virtual void OnInitUI(RHIDevice&, const OverlayFormats&) {
            // default no-op so a sample without UI is unchanged
        }

        // runs before the first pass; the acquires ride the overlay pass
        virtual std::span<const RHITextureBarrier> OnPrepareUI(RHICommandList&) {
            return {};
        }

        // runs inside the overlay pass, after its draws
        virtual void OnRecordUI(RHICommandList&) {}

        // runs before the first pass, after OnPrepareUI: the sample's own
        // GPU work, and one binding per hook pass of the running list
        virtual std::vector<PassHook> OnRecordSimulation(RHICommandList&) {
            return {};
        }

        // the lit views' clear, as a lighting key's sky; the walker is
        // rebuilt with it at the next frame
        void SetClearColor(Color color) noexcept { config.clearColor = color; }
        // the back buffer of `frame` to `path` as a BMP, with the
        // capture_frame verb's checks; why not, empty once queued
        Str RequestCapture(Str path, u64 frame);

        auto& Device() noexcept { return *device; }
        auto& Geometry() noexcept { return *geometryPool; }
        auto& Scene() noexcept { return scene; }
        auto& Renderer() noexcept { return *renderer; }
        const auto& Camera() const noexcept { return *camera; }
        auto& Camera() noexcept { return *camera; }
        RenderDebug& Debug() noexcept { return debug; }
        f32 Aspect() const noexcept { return aspect; }
        // for a sample that registers verbs of its own
        CommandPort* Port() noexcept { return port.get(); }
        // the newest frame that ended; empty before frame 1 has
        const FrameStats& LastFrameStats() const noexcept;
        // the running list's pass binding `hook`; empty when the list has
        // none, as a data view's
        std::optional<HookPassFormats> FindHook(StrView hook) const noexcept {
            return pipeline->FindHook(hook);
        }

    private:
        // a new walker from DescribePipeline(pipelineConfig)
        FramePipelinePtr describePipeline();
        void openCommandPort();
        void applyDebugFromEnvironment();
        DOM::Table controlStatus() const;
        // why the running pipeline's target `name` cannot be captured; empty
        // when it can
        Str refuseCaptureTarget(StrView name) const;
        // a path taken in either queue, or a frame taken in the one asked
        // about: the swapchain's for no target, the target's otherwise
        bool isCaptureQueued(u64 frame, StrView target, StrView path) const;
        // why `frames` cannot be captured to `paths`, the back buffer for an
        // empty target; empty when every one can
        Str refuseCaptures(StrView target, std::span<const u64> frames, std::span<const Str> paths) const;
        void queueCaptures(StrView target, std::span<const u64> frames, std::span<const Str> paths);
        // the requests due this frame, one per target, resolved by name
        std::span<TargetReadback> takeDueCaptures();
        void collectCaptures();
        // the history's entry for the frame stats.report.gpu times, if kept
        const FrameStats* gpuFrameStats(const FrameStats& stats) const noexcept;
        void answerWaits();
        void reportCullStatsOnce();
    };
}
