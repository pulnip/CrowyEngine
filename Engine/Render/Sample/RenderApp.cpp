#include "RenderApp.hpp"

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
#include "Object.hpp"
#include "ProjectFile.hpp"
#include "ProjectRoot.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    namespace
    {
        // what a debug view overrides in every material of the color passes
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
        // copy through Present over a scene colour created black, so the
        // clear always matches the one the target was created with
        StandardPipelineConfig standardConfig(
            const RenderApp::Config& config,
            const RenderDebug& debug,
            RHIPixelFormat backBufferFormat
        ) {
            StandardPipelineConfig standard{
                .depthFormat = config.depthFormat,
                .sceneColorFormat = config.sceneColorFormat,
                .clearColor = config.clearColor,
                .drawCapacity = config.drawCapacity,
                .depthPrepass =
                    wantsDepthPrepass(debug, meshPassOverride(debug)),
                .shadowMapSize = config.shadowMapSize,
                .post = config.post,
                .sceneView = debug.sceneView,
                .sceneViewFormat = backBufferFormat
            };
            if(showsData(debug.mode)) {
                standard.clearColor = Colors::Black;
                standard.post = {presentPass()};
            }

            return standard;
        }
    }

    CROWY_STRUCT(RenderDebug)
        .SetProperty("mode", &RenderDebug::mode)
        .SetProperty("wireframe", &RenderDebug::wireframe)
        .SetProperty("depthPrepass", &RenderDebug::depthPrepass)
        .SetProperty("shadowFilter", &RenderDebug::shadowFilter)
        .SetProperty("showStats", &RenderDebug::showStats)
        .SetProperty("showPanel", &RenderDebug::showPanel)
        .SetProperty("sceneView", &RenderDebug::sceneView)
    CROWY_STRUCT_END(RenderDebug)

    RenderApp::~RenderApp() = default;

    RenderApp::RenderApp(const Config& config, CameraRAII camera)
        : config(config), camera(std::move(camera)) {}

    FramePipelineDesc RenderApp::DescribePipeline(
        const StandardPipelineConfig& config
    ) {
        return makeStandardPipeline(config);
    }

    FramePipelinePtr RenderApp::describePipeline() {
        return std::make_unique<FramePipeline>(
            *device,
            DescribePipeline(pipelineConfig),
            swapchain->GetFormat(),
            swapchain->GetWidth(),
            swapchain->GetHeight()
        );
    }

    void RenderApp::OnInit(RHIDevice& device, RHISwapchain& swapchain) {
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
        meshes = std::make_unique<MeshStore>(
            [this, roots](const AssetRef& ref) {
                return loadMeshAsset(*geometryPool, roots, ref);
            },
            [this](MeshAsset asset) {
                releaseMeshAsset(*geometryPool, std::move(asset));
            }
        );
        pipelineConfig = standardConfig(config, debug, swapchain.GetFormat());
        pipeline = describePipeline();
        captures = std::make_unique<FrameCaptures>(
            device,
            swapchain,
            pipeline,
            [this] { return FrameNumber(); }
        );
        // as many view rows as the pass list names
        renderer = std::make_unique<SceneRenderer>(
            device,
            SceneRendererDesc{
                .materialCapacity = config.materialCapacity,
                .viewCount = pipeline->ViewCount()
            }
        );

        frameInputs.bindMeshPass =
            [this](RHICommandList& cmdList, const ScenePush& push) {
                OnBindPass(cmdList, push);
            };
        frameInputs.recordOverlay = [this](RHICommandList& cmdList) {
            OnRecordUI(cmdList);
        };

        OnInitUI(device, pipeline->Overlay());

        applyDebugFromEnvironment();
    #if defined(_DEBUG) || !defined(NDEBUG)
        openCommandPort();
    #endif

        // only a port verb can release the hold, so no port is no hold
        if(Runtime().hold) {
            if(port == nullptr || port->Port() == 0) {
                throw std::runtime_error(
                    "--hold needs the command port "
                    "(Debug builds, and a port that binds)"
                );
            }
            Control().Hold();
        }

        // after the port, so the scene can expose itself before the loop
        OnBuildGeometry(*geometryPool);
        geometryPool->LogAllocationStats();

        // after the geometry, because a snapshot carries the allocation
        ExtractScene(scene);
    }

    // CROWY_DEBUG: a JSON object applied to `debug` through its reflection,
    // for a run that needs a debug switch and has no port (a bench build)
    void RenderApp::applyDebugFromEnvironment() {
        const char* env = std::getenv("CROWY_DEBUG");
        if(env == nullptr || *env == '\0')
            return;

        // a bench run with a misspelled switch would measure the wrong thing
        if(const auto errors = ApplyProperties(&debug, parseJsonString(env));
           !errors.empty()) {
            Str message = "CROWY_DEBUG:";
            for(const auto& error: errors)
                message += std::format(" {};", error);
            throw std::runtime_error(message);
        }

        DOM::Value applied;
        SerializeProperties(&debug, applied);
        LOG_INFO("RenderApp", "CROWY_DEBUG: debug is {}", emitJson(applied));
    }

    // CROWY_COMMAND_PORT: unset is the default port with retries,
    // 0 disables, N forces exactly that port
    void RenderApp::openCommandPort() {
        CommandPortConfig portConfig;
        if(const char* env = std::getenv("CROWY_COMMAND_PORT")) {
            const StrView text = env;
            u16 forced = 0;
            const auto [ptr, ec] =
                std::from_chars(text.data(), text.data() + text.size(), forced);
            if(ec != std::errc{} || ptr != text.data() + text.size()) {
                LOG_WARN(
                    "RenderApp",
                    "CROWY_COMMAND_PORT='{}' is not a port number; using the default",
                    text
                );
            } else if(forced == 0) {
                return;
            } else {
                portConfig.port = forced;
                portConfig.portRetries = 0;
            }
        }

        port = std::make_unique<CommandPort>(portConfig);
        // no callback: every reader reads it each frame. Exposed even on an
        // inert port, like the samples' own targets
        port->Expose("debug", &debug, *GetDesc<RenderDebug>());
        // a port that could not bind stays, inert, so a sample can show
        // that it did not; only its verbs are skipped
        if(port->Port() == 0)
            return;

        port->RegisterVerb("ping", [this](const DOM::Value&, Reply reply) {
            auto result = controlStatus();
            result.emplace("pong", DOM::Value(true));
            result.emplace("app", DOM::Value(Runtime().window.title));
            result.emplace("elapsed", DOM::Value(ElapsedSeconds()));
            result.emplace(
                "capturesPending",
                DOM::Value(static_cast<i64>(captures->Pending()))
            );
            result.emplace(
                "captureFailures",
                DOM::Value(static_cast<i64>(captures->Failures()))
            );
            reply.Ok(DOM::Value(std::move(result)));
        });
        port->RegisterVerb("quit", [this](const DOM::Value&, Reply reply) {
            RequestQuit();
            reply.Ok(DOM::Value(DOM::Table{}));
        });
        port->RegisterVerb("run", [this](const DOM::Value& args, Reply reply) {
            const auto* frames = args.at("frames");
            const auto* until = args.at("until");
            if(frames != nullptr && until != nullptr) {
                reply.Error("run takes frames or until, not both");

                return;
            }

            if(frames != nullptr) {
                const auto count = parsePositiveInteger(frames);
                if(!count) {
                    reply.Error("frames must be a positive integer");

                    return;
                }
                Control().RunFrames(*count);
            } else if(until != nullptr) {
                const auto frame = parsePositiveInteger(until);
                if(!frame) {
                    reply.Error("until must be a positive integer");

                    return;
                }
                if(*frame <= FrameNumber()) {
                    reply.Error(std::format(
                        "frame {} has already ended (the last frame is {})",
                        *frame,
                        FrameNumber()
                    ));

                    return;
                }
                Control().RunUntil(*frame);
            } else {
                Control().Run();
            }

            reply.Ok(DOM::Value(controlStatus()));
        });
        // the gate runs right after this drain, so the loop stops at the
        // frame that has just ended
        port->RegisterVerb("hold", [this](const DOM::Value&, Reply reply) {
            Control().Hold();
            reply.Ok(DOM::Value(controlStatus()));
        });
        port->RegisterVerb("step", [this](const DOM::Value&, Reply reply) {
            Control().RunFrames(1);
            reply.Ok(DOM::Value(controlStatus()));
        });
        // answers once `frame` has ended; it never advances the loop itself
        port->RegisterVerb(
            "wait_frame",
            [this](const DOM::Value& args, Reply reply) {
                const auto frame = parsePositiveInteger(args.at("frame"));
                if(!frame) {
                    reply.Error("\"frame\" is missing or not a positive integer");

                    return;
                }
                if(*frame <= FrameNumber()) {
                    reply.Ok(DOM::Value(controlStatus()));

                    return;
                }

                pendingWaits.push_back(
                    PendingWait{.frame = *frame, .reply = std::move(reply)}
                );
            }
        );
        captures->RegisterVerbs(*port);
        stats.RegisterVerbs(*port, [this] { return FrameNumber(); });
        // every cached pipeline, recompiled from disk; a failed compile keeps
        // the old ones drawing and throws its diagnostic into the error reply
        port->RegisterVerb(
            "reload_shaders",
            [this](const DOM::Value&, Reply reply) {
                const auto rebuild = renderer->ReloadPipelines();
                const auto programs = OnReloadShaders();

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
                reply.Ok(DOM::Value(std::move(result)));
            }
        );
    }

    // every control verb answers with where the loop stands afterwards
    DOM::Table RenderApp::controlStatus() const {
        const auto frame = FrameNumber();
        const auto holdAt = Control().HoldAt(frame);

        DOM::Table status;
        status.emplace("frame", DOM::Value(static_cast<i64>(frame)));
        status.emplace("held", DOM::Value(Control().IsHeld(frame)));
        status.emplace(
            "holdAt",
            holdAt ? DOM::Value(static_cast<i64>(*holdAt)) : DOM::Value()
        );

        return status;
    }

    void RenderApp::answerWaits() {
        const auto frame = FrameNumber();
        for(auto& wait: pendingWaits) {
            if(wait.frame <= frame)
                wait.reply.Ok(DOM::Value(controlStatus()));
        }
        std::erase_if(pendingWaits, [frame](const PendingWait& wait) {
            return wait.frame <= frame;
        });
    }

    void RenderApp::NewFrame() {
        if(port == nullptr)
            return;

        // the completions go out on this same drain
        captures->Collect();
        answerWaits();
        port->Drain();
    }

    void RenderApp::ProcessInput(const InputProvider& input) {
        camera->ProcessInput(input);
        OnProcessInput(input);
    }

    void RenderApp::OnUpdate(f64 deltaTime, f64) {
        camera->Update(deltaTime);
        OnUpdateScene(deltaTime);
        // after the scene's own Sync, so what it let go leaves this frame
        meshes->Collect();
    }

    void RenderApp::OnBindPass(RHICommandList& cmdList, const ScenePush& push) {
        cmdList.SetPushGraphicsConstants(push);
    }

    // does not prove the culling did anything.
    void RenderApp::reportCullStatsOnce() {
        if(reportedCullStats)
            return;

        reportedCullStats = true;
        LOG_INFO(
            "RenderApp",
            "first frame: {} of {} primitives survived culling, "
            "{} draws in {} runs over {} pipelines",
            stats.Recorded().visiblePrimitives,
            scene.Primitives().Count(),
            stats.Recorded().draws,
            stats.Recorded().runs,
            renderer->PipelineCount()
        );
    }

    void RenderApp::OnRecord(
        RHICommandList& cmdList,
        const RHIColorAttachment& backBuffer
    ) {
        // one copy for the whole frame: OnPrepareUI below may write the
        // original, and the pipelines and the clear must agree
        const auto frameDebug = debug;
        const auto overrides = meshPassOverride(frameDebug);

        // the prepass and the post list are passes in the list, so turning
        // either over is a new list; frames in flight keep reading the old
        // walker's targets
        auto wanted =
            standardConfig(config, frameDebug, swapchain->GetFormat());
        if(wanted != pipelineConfig) {
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
        OnUpdateFrameData();

        renderer->BeginFrame(scene, pipeline->ShadowMapSize());
        pipeline->Prepare(*renderer, scene, overrides);
        renderer->Upload();

        stats.Begin(renderer->Visible(ViewMain).primitiveCount);

        frameInputs.backBuffer = backBuffer.texture;
        frameInputs.sceneClear = pipelineConfig.clearColor;
        frameInputs.indices =
            RHIIndexBufferView{.buffer = &geometryPool->GetIndexBuffer()};
        // the pool is GPUOnly, so unlike the renderer's own buffers this one
        // does not have to be re-resolved
        frameInputs.vertices = geometryPool->GetVertexBufferID();
        // both outside any pass: the pool's copies, then the UI's
        frameInputs.geometryAcquires = geometryPool->RecordUploads(cmdList);
        frameInputs.overlayAcquires = OnPrepareUI(cmdList);
        frameHooks = OnRecordSimulation(cmdList);
        frameInputs.hooks = frameHooks;
        // resolved after any rebuild above, so a request keeps its name
        frameInputs.frame = FrameNumber();
        frameInputs.captures = captures->TakeDue();

        pipeline->Record(cmdList, *renderer, frameInputs);

        captures->HandOff();
        frameInputs.captures = {};
        frameInputs.hooks = {};
        frameHooks.clear();

        stats.AddPasses(pipeline->Stats());
        reportCullStatsOnce();
    }

    void RenderApp::OnFrameEnd(const FrameReport& report) {
        CROWY_ASSERT(report.frame == FrameNumber(),
            "the profiler and the app count different frames"
        );

        stats.End(
            FrameNumber(),
            report,
            scene.Primitives().Count(),
            renderer->PipelineCount()
        );
    }

    void RenderApp::OnResize(u32 width, u32 height) {
        pipeline->Resize(width, height);
    }

    void RenderApp::SetSceneViewSize(u32 width, u32 height) noexcept {
        if(width == 0 || height == 0)
            return;

        sceneViewSize = Size2D{width, height};
    }

    u64 RenderApp::SceneViewID() const {
        const auto id = pipeline->FindTarget(SceneViewName);

        return id ? pipeline->OverlayReadableID(*id) : 0;
    }
}
