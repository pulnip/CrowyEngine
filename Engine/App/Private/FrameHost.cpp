#include "FrameHost.hpp"

#include <algorithm>
#include <charconv>
#include <cstdlib>
#include <format>
#include <span>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#include "ClassRegistry.hpp"
#include "FrameSelector.hpp"
#include "JsonLoader.hpp"
#include "Log.hpp"
#include "ProjectFile.hpp"
#include "ProjectRoot.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"
#include "StringUtil.hpp"

namespace Crowy
{
    namespace
    {
        MeshPassOverride meshPassOverride(const RenderDebug& debug) {
            MeshPassOverride overrides;
            if(debug.wireframe)
                overrides.fillMode = RHIFillMode::Wireframe;
            if(debug.mode == DebugMode::Overdraw) {
                // every fragment adds; blendEnable is spelled out because
                // Metal blends without it and D3D12 would not
                RHIBlendState additive{};
                additive.renderTargets[0] = RHIRenderTargetBlendState{
                    .blendEnable = true,
                    .srcBlend = RHIBlend::One,
                    .dstBlend = RHIBlend::One,
                    .blendOp = RHIBlendOp::Add
                };
                overrides.blend = additive;
                overrides.depthFunc = RHIComparisonFunc::Always;
                overrides.depthWrite = false;
            }

            return overrides;
        }

        // Overdraw tests Always, so a prepass would only cost; a wireframe
        // one stores line depths, which Equal cannot test lines against
        bool wantsDepthPrepass(
            const RenderDebug& debug,
            const MeshPassOverride& overrides
        ) {
            return debug.depthPrepass && !overrides.fillMode &&
                   !overrides.depthFunc && !overrides.depthWrite;
        }

        // a view whose pixels are data, which no tone map or stylized entry
        // may distort; additive Overdraw counts read only against black
        bool showsData(DebugMode mode) {
            using enum DebugMode;

            switch(mode) {
            case Lit:
            case Unshaded:
                return false;
            case Normals:
            case Depth:
            case Overdraw:
            case Shadow:
                return true;
            }

            return false;
        }

        // the lit views take the sample's clear and post list; the data views
        // copy through Present over a scene color cleared black
        StandardFrameConfig standardConfig(
            const FrameHost::Config& config,
            const RenderDebug& debug,
            RHIPixelFormat backBufferFormat
        ) {
            StandardFrameConfig standard{
                .depthFormat = config.depthFormat,
                .sceneColorFormat = config.sceneColorFormat,
                .clearColor = config.clearColor,
                .drawCapacity = config.drawCapacity,
                .depthPrepass =
                    wantsDepthPrepass(debug, meshPassOverride(debug)),
                .shadowMapSize = config.shadowMapSize,
                .post = config.post,
                .sceneView = debug.sceneView && config.showsSceneView,
                .sceneViewFormat = backBufferFormat
            };
            if(showsData(debug.mode)) {
                standard.clearColor = Colors::Black;
                standard.post = {presentPass()};
            }

            return standard;
        }
    }

    FrameHost::~FrameHost() = default;

    FrameHost::FrameHost(
        FrameContent& content,
        const Config& config,
        CameraRAII camera
    )
        : content(content), config(config), camera(std::move(camera)) {}

    FrameGraphRAII FrameHost::describePipeline() {
        return std::make_unique<FrameGraph>(
            *device,
            content.DescribePipeline(pipelineConfig),
            swapchain->GetFormat(),
            swapchain->GetWidth(),
            swapchain->GetHeight()
        );
    }

    void FrameHost::Init(
        RHIDevice& device,
        RHISwapchain& swapchain,
        FrameLoop loop,
        bool hold
    ) {
        frameNumber = loop.frame;
        this->device = &device;
        this->swapchain = &swapchain;

        aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();

        geometryPool = std::make_unique<GeometryPool>(
            device,
            config.vertexPoolCapacity,
            config.indexPoolCapacity
        );
        const AssetRoots roots{
            .engine = projectRoot() / "Engine",
            .content = resolveContentPath("")
        };
        session = &content.OnOpenSession(device, *geometryPool, roots);
        pipelineConfig = standardConfig(config, debug, swapchain.GetFormat());
        pipeline = describePipeline();
        captures = std::make_unique<FrameCaptures>(
            device,
            swapchain,
            pipeline,
            frameNumber
        );
        framePort = std::make_unique<FramePort>(std::move(loop), *captures);
        renderer = std::make_unique<SceneRenderer>(
            device,
            SceneRendererDesc{
                .materialCapacity = config.materialCapacity,
                .viewCount = pipeline->ViewCount()
            }
        );

        frameInputs.bindMeshPass =
            [this](RHICommandList& cmdList, const ScenePush& push) {
                content.OnBindPass(cmdList, push);
            };
        frameInputs.recordOverlay = [this](RHICommandList& cmdList) {
            content.OnRecordUI(cmdList);
        };

        content.OnInitUI(device, pipeline->Overlay());

        applyDebugFromEnvironment();
#if CROWY_DEBUG_BUILD
        openCommandPort();
#endif

        if(hold)
            framePort->Hold();
        framePort->CheckPortInput();

        // after the port, so the scene can expose itself before the loop
        content.OnBuildGeometry(*geometryPool);
        geometryPool->LogAllocationStats();

        // after the geometry, because a snapshot carries the allocation
        content.ExtractScene(session->Scene());
    }

    // CROWY_DEBUG: a JSON object applied to `debug` through its reflection,
    // for a run that needs a debug switch and has no port (a bench build)
    void FrameHost::applyDebugFromEnvironment() {
        const char* env = std::getenv("CROWY_DEBUG");
        if(env == nullptr || *env == '\0')
            return;

        // a bench run with a misspelled switch would measure the wrong thing
        if(const auto errors = applyProperties(&debug, parseJsonString(env));
           !errors.empty()) {
            throw std::runtime_error(
                std::format("CROWY_DEBUG: {};", join(errors, "; "))
            );
        }
        if(debug.sceneView && !config.showsSceneView) {
            throw std::runtime_error(
                "CROWY_DEBUG: sceneView needs a sample that draws the scene "
                "view;"
            );
        }

        DOM::Value applied;
        serializeProperties(&debug, applied);
        LOG_INFO("RenderApp", "CROWY_DEBUG: debug is {}", emitJson(applied));
    }

    void FrameHost::refuseSceneView() {
        if(!debug.sceneView || config.showsSceneView)
            return;

        debug.sceneView = false;
        LOG_WARN(
            "RenderApp",
            "debug.sceneView needs a sample that draws the scene view; it "
            "stays off"
        );
    }

    void FrameHost::openCommandPort() {
        framePort->Open();
        auto* port = framePort->Port();
        if(port == nullptr)
            return;

        // every reader reads it each frame, so the callback only refuses a
        // scene view; exposed even on an inert port, like the samples' own
        port->Expose("debug", &debug, *getDesc<RenderDebug>(), [this] {
            refuseSceneView();
        });
        // a port that could not bind stays, inert, so a sample can show
        // that it did not; only its verbs are skipped
        if(port->Port() == 0)
            return;

        framePort->RegisterVerbs();
        captures->RegisterVerbs(*port);
        stats.RegisterVerbs(*port, frameNumber);
        // every cached pipeline, recompiled from disk; a failed compile keeps
        // the old ones drawing and throws its diagnostic into the error reply
        port->RegisterVerb(
            "reload_shaders",
            {},
            [this](const DOM::Value&, Reply reply) {
                const auto rebuild = renderer->ReloadPipelines();
                const auto programs = content.OnReloadShaders();

                DOM::Table result;
                result.emplace(
                    "pipelines",
                    DOM::Value(static_cast<i64>(rebuild.pipelines))
                );
                result.emplace(
                    "programs",
                    DOM::Value(static_cast<i64>(programs))
                );
                result.emplace("ms", DOM::Value(rebuild.milliseconds));
                reply.Ok(std::move(result));
            }
        );
    }

    void FrameHost::NewFrame() {
        if(framePort->Port() == nullptr)
            return;

        // the completions go out on this same drain
        captures->Collect();
        framePort->Drain();
    }

    void FrameHost::ProcessInput(const InputProvider& input) {
        relativeMouse = false;
        camera->ProcessInput(input);
        content.OnProcessInput(input);
    }

    void FrameHost::Update(f64 deltaTime) {
        camera->Update(deltaTime);
        content.OnUpdateScene(deltaTime);
        // after the scene's own Sync, so what it let go leaves this frame
        session->Collect();
    }

    void FrameHost::reportCullStatsOnce() {
        if(reportedCullStats)
            return;

        reportedCullStats = true;
        LOG_INFO(
            "RenderApp",
            "first frame: {} of {} primitives survived culling, "
            "{} draws in {} runs over {} pipelines",
            stats.Recorded().visiblePrimitives,
            session->Scene().Primitives().Count(),
            stats.Recorded().draws,
            stats.Recorded().runs,
            renderer->PipelineCount()
        );
    }

    void FrameHost::Record(
        RHICommandList& cmdList,
        const RHIColorAttachment& backBuffer
    ) {
        // one copy for the whole frame: OnPrepareUI below may write the
        // original, and the pipelines and the clear must agree
        const auto frameDebug = debug;
        const auto overrides = meshPassOverride(frameDebug);
        auto& scene = session->Scene();

        // turning the prepass or the post list over is a new list; frames in
        // flight keep reading the old walker's targets
        auto wanted =
            standardConfig(config, frameDebug, swapchain->GetFormat());
        if(const auto& sky = scene.Environment().sky;
           sky && !showsData(frameDebug.mode)) {
            wanted.clearColor = toVec4(*sky, 1.0f);
        }
        // the walker clears with this every frame; the list only keeps the
        // value its scene color was created with
        const auto sceneClear = wanted.clearColor;
        wanted.clearColor = pipelineConfig.clearColor;
        if(wanted != pipelineConfig) {
            wanted.clearColor = sceneClear;
            pipelineConfig = std::move(wanted);
            auto rebuilt = describePipeline();
            CROWY_ASSERT(
                rebuilt->Overlay() == pipeline->Overlay(),
                "the UI's formats are frozen at OnInitUI"
            );
            CROWY_ASSERT(
                rebuilt->ViewCount() == renderer->ViewCount(),
                "the renderer's view rows are sized at OnInit"
            );
            pipeline = std::move(rebuilt);
        }

        // a rebuilt list starts at the back buffer's size, so this follows
        // every rebuild too
        if(const auto extent = pipeline->FindExtent(SceneViewName);
           extent && sceneViewSize.x != 0) {
            pipeline->ResizeExtent(*extent, sceneViewSize.x, sceneViewSize.y);
        }
        const auto sceneSize = pipeline->TargetSize(pipeline->SceneColor());
        aspect = static_cast<f32>(sceneSize.x) / sceneSize.y;

        auto& view = renderer->View(ViewMain);
        view.viewProj = camera->ViewProj(aspect);
        view.debugMode = static_cast<u32>(frameDebug.mode);
        view.shadowFilter = static_cast<u32>(frameDebug.shadowFilter);
        view.cameraPosition = toVec4(camera->Position(), 1.0f);

        // every per-frame buffer settles before the first pass opens
        content.OnUpdateFrameData();

        renderer->BeginFrame(scene, pipeline->ShadowMapSize());
        pipeline->Prepare(*renderer, scene, overrides);
        renderer->Upload();

        stats.Begin(renderer->Visible(ViewMain).primitiveCount);

        frameInputs.backBuffer = backBuffer.texture;
        frameInputs.sceneClear = sceneClear;
        frameInputs.indices =
            RHIIndexBufferView{.buffer = &geometryPool->GetIndexBuffer()};
        // the pool is GPUOnly, so unlike the renderer's own buffers this one
        // does not have to be re-resolved
        frameInputs.vertices = geometryPool->GetVertexBufferID();
        // both outside any pass: the pool's copies, then the UI's
        frameInputs.geometryAcquires = geometryPool->RecordUploads(cmdList);
        frameInputs.overlayAcquires = content.OnPrepareUI(cmdList);
        frameHooks = content.OnRecordSimulation(cmdList);
        frameInputs.hooks = frameHooks;
        // resolved after any rebuild above, so a request keeps its name
        frameInputs.frame = frameNumber();
        frameInputs.captures = captures->TakeDue();

        pipeline->Record(cmdList, *renderer, frameInputs);

        captures->HandOff();
        frameInputs.captures = {};
        frameInputs.hooks = {};
        frameHooks.clear();

        stats.AddPasses(pipeline->Stats());
        reportCullStatsOnce();
    }

    void FrameHost::FrameEnd(const FrameReport& report) {
        CROWY_ASSERT(
            report.frame == frameNumber(),
            "the profiler and the app count different frames"
        );

        stats.End(
            frameNumber(),
            report,
            session->Scene().Primitives().Count(),
            renderer->PipelineCount()
        );
    }

    void FrameHost::Resize(u32 width, u32 height) {
        pipeline->Resize(width, height);
    }

    void FrameHost::SetSceneViewSize(u32 width, u32 height) noexcept {
        if(width == 0 || height == 0)
            return;

        sceneViewSize = Size2D{width, height};
    }

    u64 FrameHost::SceneViewID() const {
        const auto id = pipeline->FindTarget(SceneViewName);

        return id ? pipeline->OverlayReadableID(*id) : 0;
    }

    // clang-format off
    CROWY_STRUCT(RenderDebug)
        .AddProperty("mode", &RenderDebug::mode)
        .AddProperty("wireframe", &RenderDebug::wireframe)
        .AddProperty("depthPrepass", &RenderDebug::depthPrepass)
        .AddProperty("shadowFilter", &RenderDebug::shadowFilter)
        .AddProperty("showStats", &RenderDebug::showStats)
        .AddProperty("showPanel", &RenderDebug::showPanel)
        .AddProperty("sceneView", &RenderDebug::sceneView)
    CROWY_STRUCT_END(RenderDebug)
    // clang-format on
}
