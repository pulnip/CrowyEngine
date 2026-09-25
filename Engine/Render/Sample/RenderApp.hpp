#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "AppFramework.hpp"
#include "Camera.hpp"
#include "CommandPort.hpp"
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
    using GeometryPoolPtr = RAII<GeometryPool>;
    using SceneRendererPtr = RAII<SceneRenderer>;
    using CommandPortPtr = RAII<CommandPort>;

    // Sample framework for Engine/Render: Owns the frame order and seals it
    class RenderApp: public App {
    public:
        // the sample's statement of what its capture is taken against
        struct Config {
            RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
            Color clearColor = Colors::Black;

            // worst cases, not live counts
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
            usize buckets = 0;
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
        // storage the pass description's span points at
        RHIPixelFormat colorFormat = RHIPixelFormat::RGBA8_UNORM;
        f32 aspect = 1.0f;

        GeometryPoolPtr geometryPool;
        SceneRendererPtr renderer;
        RenderScene scene;
        CameraRAII camera;

        // debug builds only; null when disabled, inert with
        // Status().server == BindFailed when no port could bind
        CommandPortPtr port;
        // capture_frame answers only once the swapchain reports the file
        // written, so its reply waits here across frames
        std::optional<Reply> pendingCapture;
        Str pendingCapturePath;
        // a wait the port already timed out stays until its frame ends, and
        // answering it then does nothing
        std::vector<PendingWait> pendingWaits;
        FrameStatsHistory frameStats;

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
        f32 Aspect() const noexcept { return aspect; }
        // for a sample that registers verbs of its own
        CommandPort* Port() noexcept { return port.get(); }
        // the newest frame that ended; empty before frame 1 has
        const FrameStats& LastFrameStats() const noexcept;

    private:
        void createDepthBuffer(u32 width, u32 height);
        void openCommandPort();
        DOM::Table controlStatus() const;
        void pollCapture();
        void answerWaits();
        void reportCullStatsOnce();
    };
}
