#pragma once

#include <memory>
#include <span>
#include <vector>

#include "AppFramework.hpp"
#include "Camera.hpp"
#include "CommandPort.hpp"
#include "EnumUtil.hpp"
#include "FrameHistory.hpp"
#include "FrameProfiler.hpp"
#include "GeometryPool.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"

namespace Crowy
{
    using DrawListPtr = RAII<DrawList>;
    using GeometryPoolPtr = RAII<GeometryPool>;
    using SceneRendererPtr = RAII<SceneRenderer>;
    using CommandPortPtr = RAII<CommandPort>;

    // mirrored by the constants in Engine/Shader/DebugView.slang
    enum class DebugMode : u32 {
        Lit,
        Unshaded,
        Normals,
        Depth,
        Overdraw,
    };

    CROWY_ENUM_BEGIN(DebugMode)
        CROWY_ENUM_VALUE(Lit)
        CROWY_ENUM_VALUE(Unshaded)
        CROWY_ENUM_VALUE(Normals)
        CROWY_ENUM_VALUE(Depth)
        CROWY_ENUM_VALUE(Overdraw)
    CROWY_ENUM_END()

    // what the frame shows instead of the plain lit picture, exposed as `debug`
    struct RenderDebug {
        DebugMode mode = DebugMode::Lit;
        bool wireframe = false;
        // off, the opaque round tests Less and writes depth itself; a view
        // that overrides fill mode or depth draws without a prepass anyway
        bool depthPrepass = true;
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
            Color clearColor = Colors::Black;

            // reserves: the scratch grows, and the transient ring is the limit
            u32 drawCapacity = 4096;
            u32 materialCapacity = 256;
            u32 viewCount = 1;

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
        };

        static constexpr u32 ViewMain = 0;
        static constexpr u32 ViewCBSlot = 0;

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
        RHITextureRAII depthBuffer;
        // the swapchain's, which the scene pass renders to
        RHIPixelFormat colorFormat = RHIPixelFormat::RGBA8_UNORM;
        f32 aspect = 1.0f;

        GeometryPoolPtr geometryPool;
        SceneRendererPtr renderer;
        // one per mesh round of the frame
        DrawListPtr prepassList;
        DrawListPtr opaqueList;
        DrawListPtr translucentList;
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

        // this frame's counts, summed over its lists; OnFrameEnd adds the
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

        // Override to push a struct starting with the same members
        // when a shader wants more root constants.
        virtual void OnBindPass(RHICommandList& cmdList, const ScenePush& push);

        // The sample's own per-frame buffers, written here
        // because the pass has not opened yet.
        virtual void OnUpdateFrameData() {}

        // input a sample reads beyond the camera's
        virtual void OnProcessInput(const InputProvider&) {}

        virtual void OnInitUI(
            RHIDevice&,
            RHIPixelFormat colorFormat,
            RHIPixelFormat depthFormat
        ) {
            // default no-op so a sample without UI is unchanged
        }

        // runs before the render pass
        virtual std::span<const RHITextureBarrier> OnPrepareUI(RHICommandList&) {
            return {};
        }

        // runs inside the pass, after the scene submit
        virtual void OnRecordUI(RHICommandList&) {}

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

    private:
        void createDepthBuffer(u32 width, u32 height);
        void openCommandPort();
        void applyDebugFromEnvironment();
        DOM::Table controlStatus() const;
        void collectCaptures();
        // the history's entry for the frame stats.report.gpu times, if kept
        const FrameStats* gpuFrameStats(const FrameStats& stats) const noexcept;
        void answerWaits();
        void reportCullStatsOnce();
        // builds and uploads a list over the main view; its counts join the
        // frame's
        void buildList(
            DrawList& list,
            const PassPipelineDesc& pass,
            const DrawFilter& filter,
            DrawOrder order
        );
        // a list's view, push and draws, in the open pass
        void submitList(RHICommandList& cmdList, const DrawList& list);
    };
}
