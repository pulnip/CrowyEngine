#pragma once

#include <memory>
#include <optional>
#include <span>
#include <vector>

#include "Camera.hpp"
#include "CommandPort.hpp"
#include "DebugMode.hpp"
#include "EnumUtil.hpp"
#include "FrameCaptures.hpp"
#include "FrameContent.hpp"
#include "FrameGraph.hpp"
#include "FramePort.hpp"
#include "FrameProfiler.hpp"
#include "FrameStatsLog.hpp"
#include "GeometryPool.hpp"
#include "InputProvider.hpp"
#include "MeshAsset.hpp"
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "RenderScene.hpp"
#include "SceneRenderer.hpp"
#include "Semantics.hpp"
#include "StandardFrame.hpp"

namespace Crowy
{
    using FrameGraphPtr = RAII<FrameGraph>;
    using GeometryPoolPtr = RAII<GeometryPool>;
    using MeshStorePtr = RAII<MeshStore>;
    using SceneRendererPtr = RAII<SceneRenderer>;
    using FrameCapturesPtr = RAII<FrameCaptures>;
    using FramePortPtr = RAII<FramePort>;

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
        // the scene drawn into a target the sample's UI shows, sized by
        // SetSceneViewSize; refused where Config::showsSceneView is off
        bool sceneView = false;

        friend bool operator==(const RenderDebug&, const RenderDebug&) =
            default;
    };

    class FrameHost {
    public:
        struct Config {
            RHIPixelFormat depthFormat = RHIPixelFormat::D32_FLOAT;
            // what the color passes write; RGBA8_UNORM for a scene already
            // in display values
            RHIPixelFormat sceneColorFormat = RHIPixelFormat::RGBA16_FLOAT;
            // the lit views' scene color; the data views clear black
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

            // the UI draws SceneViewID(); without it the window would only
            // get the UI pass over black
            bool showsSceneView = false;
        };

        static constexpr u32 ViewMain = 0;

    private:
        FrameContent& content;
        Config config;
        // the loop's last frame that ended, from Init on
        FrameStatsLog::LastFrame frameNumber;

        RHIDevice* device = nullptr;
        RHISwapchain* swapchain = nullptr;
        // the scene's, from its color target
        f32 aspect = 1.0f;
        // the scene view's, applied at the next frame; 0 keeps the back
        // buffer's
        Size2D sceneViewSize;

        GeometryPoolPtr geometryPool;
        // after the pool it frees into, before the scene whose rows hold it
        MeshStorePtr meshes;
        SceneRendererPtr renderer;
        // what DescribePipeline was last asked for, from config and the debug
        // view; a change rebuilds it
        StandardFrameConfig pipelineConfig;
        FrameGraphPtr pipeline;
        // bindMeshPass and recordOverlay are set once; the rest, hooks
        // included, is refilled every frame
        FrameInputs frameInputs;
        RenderScene scene;
        CameraRAII camera;
        // declared before the port, which points at it, so it outlives it
        RenderDebug debug;

        FrameStatsLog stats;
        FrameCapturesPtr captures;
        // its port opens in debug builds only; last, so its verbs go first
        FramePortPtr framePort;
        // OnRecordSimulation's, alive until the walker has recorded
        std::vector<PassHook> frameHooks;

        // this frame's, asked by a look while it turns
        bool relativeMouse = false;
        bool reportedCullStats = false;

    public:
        ~FrameHost();
        CROWY_DECLARE_PINNED(FrameHost)

        FrameHost(
            FrameContent& content,
            const Config& config,
            CameraRAII camera
        );

        // `hold` is --hold: the loop waits for the port from frame 1
        void Init(
            RHIDevice& device,
            RHISwapchain& swapchain,
            FrameLoop loop,
            bool hold
        );
        void NewFrame();
        void ProcessInput(const InputProvider& input);
        void Update(f64 deltaTime);
        void Record(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        );
        void FrameEnd(const FrameReport& report);
        void Resize(u32 width, u32 height);

        // the lit views' clear, as a lighting key's sky, from the next frame;
        // the pipeline stays as it is
        void SetClearColor(Color color) noexcept { config.clearColor = color; }
        // the scene view's size from the next frame on; a zero side is
        // ignored, as a collapsed window's
        void SetSceneViewSize(u32 width, u32 height) noexcept;
        // the scene view target for the UI to sample this frame; 0 without
        // debug.sceneView
        u64 SceneViewID() const;
        // the cursor hidden for this frame, as a look turning the camera
        // asks; asked again every frame it holds
        void RequestRelativeMouse() noexcept { relativeMouse = true; }
        bool WantsRelativeMouse() const noexcept { return relativeMouse; }
        // the back buffer of `frame` to `path` as a BMP, with the
        // capture_frame verb's checks; why not, empty once queued
        Str RequestCapture(Str path, u64 frame) {
            return captures->Request(std::move(path), frame);
        }

        auto& Device() noexcept { return *device; }
        auto& Geometry() noexcept { return *geometryPool; }
        auto& Meshes() noexcept { return *meshes; }
        auto& Scene() noexcept { return scene; }
        auto& Renderer() noexcept { return *renderer; }
        const auto& Camera() const noexcept { return *camera; }
        auto& Camera() noexcept { return *camera; }
        RenderDebug& Debug() noexcept { return debug; }
        f32 Aspect() const noexcept { return aspect; }
        // for a sample that registers verbs of its own
        CommandPort* Port() noexcept {
            return framePort != nullptr ? framePort->Port() : nullptr;
        }
        // the newest frame that ended; empty before frame 1 has
        const FrameStats& LastFrameStats() const noexcept {
            return stats.Last();
        }
        // the formats of the running list's pass naming `hook`; empty when
        // the list has none, as a data view's
        std::optional<HookPassFormats> FindHook(StrView hook) const noexcept {
            return pipeline->FindHook(hook);
        }

    private:
        FrameGraphPtr describePipeline();
        void openCommandPort();
        void applyDebugFromEnvironment();
        // a port write of debug.sceneView where nothing draws the view
        void refuseSceneView();
        void reportCullStatsOnce();
    };
}
