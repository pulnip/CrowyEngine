#include "RenderApp.hpp"

#include <algorithm>
#include <array>
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
        constexpr StrView FramePlaceholder = "{frame}";

        // "captures/f-{frame}.bmp" names frame 60 "captures/f-60.bmp"
        Str withFrame(StrView path, u64 frame) {
            const auto number = std::to_string(frame);

            Str named;
            usize at = 0;
            for(auto found = path.find(FramePlaceholder);
                found != StrView::npos;
                found = path.find(FramePlaceholder, at)) {
                named += path.substr(at, found - at);
                named += number;
                at = found + FramePlaceholder.size();
            }
            named += path.substr(at);

            return named;
        }

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
            const RenderDebug& debug
        ) {
            StandardPipelineConfig standard{
                .depthFormat = config.depthFormat,
                .sceneColorFormat = config.sceneColorFormat,
                .clearColor = config.clearColor,
                .drawCapacity = config.drawCapacity,
                .depthPrepass =
                    wantsDepthPrepass(debug, meshPassOverride(debug)),
                .shadowMapSize = config.shadowMapSize,
                .post = config.post
            };
            if(showsData(debug.mode)) {
                standard.clearColor = Colors::Black;
                standard.post = {presentPass()};
            }

            return standard;
        }

        // every number describes stats.report.frame, one frame that ended,
        // except gpu: the newest GPU time then, with what that frame drew
        // from gpuFrame; the time alone once gpuFrame has aged out
        DOM::Value statsToDom(
            const RenderApp::FrameStats& stats,
            const RenderApp::FrameStats* gpuFrame
        ) {
            constexpr auto MsPerSecond = 1000.0;

            DOM::Table result;
            result.emplace(
                "frame",
                DOM::Value(static_cast<i64>(stats.report.frame))
            );
            result.emplace(
                "primitives",
                DOM::Value(static_cast<i64>(stats.primitives))
            );
            result.emplace(
                "visiblePrimitives",
                DOM::Value(static_cast<i64>(stats.visiblePrimitives))
            );
            result.emplace(
                "triangles",
                DOM::Value(static_cast<i64>(stats.triangles))
            );
            result.emplace(
                "draws",
                DOM::Value(static_cast<i64>(stats.draws))
            );
            result.emplace(
                "runs",
                DOM::Value(static_cast<i64>(stats.runs))
            );
            result.emplace(
                "pipelines",
                DOM::Value(static_cast<i64>(stats.pipelines))
            );
            result.emplace("instrumented", DOM::Value(static_cast<bool>(CROWY_FRAME_STATS)));

            DOM::Array passes;
            for(const auto& pass: stats.passes) {
                DOM::Table row;
                row.emplace("name", DOM::Value(pass.name));
                row.emplace("draws", DOM::Value(static_cast<i64>(pass.draws)));
                row.emplace("runs", DOM::Value(static_cast<i64>(pass.runs)));
                row.emplace(
                    "triangles",
                    DOM::Value(static_cast<i64>(pass.triangles))
                );
                row.emplace(
                    "barrierEdges",
                    DOM::Value(static_cast<i64>(pass.barrierEdges))
                );
                passes.push_back(DOM::Value(std::move(row)));
            }
            result.emplace("passes", DOM::Value(std::move(passes)));

            DOM::Table cpu;
            for(usize i = 0; i < NUM_FRAME_SECTION; ++i) {
                cpu.emplace(
                    Str{ToString(static_cast<FrameSection>(i))} + "Ms",
                    DOM::Value(stats.report.seconds[i] * MsPerSecond)
                );
            }
            result.emplace(
                "cpu",
                DOM::Value(std::move(cpu))
            );

            const auto& s = stats.report.rhi;
            DOM::Table rhi;
            rhi.emplace(
                "commandListBegins",
                DOM::Value(static_cast<i64>(s.commandListBeginCount))
            );
            rhi.emplace(
                "commandListCreates",
                DOM::Value(static_cast<i64>(s.commandListCreateCount))
            );
            rhi.emplace(
                "renderPasses",
                DOM::Value(static_cast<i64>(s.renderPassCount))
            );
            rhi.emplace(
                "computePasses",
                DOM::Value(static_cast<i64>(s.computePassCount))
            );
            rhi.emplace(
                "blitPasses",
                DOM::Value(static_cast<i64>(s.blitPassCount))
            );
            rhi.emplace(
                "directDraws",
                DOM::Value(static_cast<i64>(s.drawCount))
            );
            rhi.emplace(
                "indirectBatches",
                DOM::Value(static_cast<i64>(s.indirectBatchCount))
            );
            rhi.emplace(
                "indirectDraws",
                DOM::Value(static_cast<i64>(s.indirectDrawCount))
            );
            rhi.emplace(
                "dispatches",
                DOM::Value(static_cast<i64>(s.dispatchCount))
            );
            rhi.emplace(
                "copies",
                DOM::Value(static_cast<i64>(s.copyCount))
            );
            rhi.emplace(
                "pipelineSets",
                DOM::Value(static_cast<i64>(s.pipelineSetCount))
            );
            rhi.emplace(
                "constantBufferSets",
                DOM::Value(static_cast<i64>(s.constantBufferSetCount))
            );
            rhi.emplace(
                "pushConstantSets",
                DOM::Value(static_cast<i64>(s.pushConstantSetCount))
            );
            rhi.emplace(
                "vertexBufferSets",
                DOM::Value(static_cast<i64>(s.vertexBufferSetCount))
            );
            rhi.emplace(
                "barrierEdges",
                DOM::Value(static_cast<i64>(s.barrierEdgeCount))
            );
            result.emplace(
                "rhi",
                DOM::Value(std::move(rhi))
            );

            if(!stats.report.gpu) {
                result.emplace("gpu", DOM::Value());

                return DOM::Value(std::move(result));
            }

            const auto& time = *stats.report.gpu;
            DOM::Table gpu;
            gpu.emplace("frame", DOM::Value(static_cast<i64>(time.frame)));
            gpu.emplace("frameMs", DOM::Value(time.seconds * MsPerSecond));
            if(gpuFrame != nullptr) {
                const auto& drawn = gpuFrame->report.rhi;
                gpu.emplace(
                    "directDraws",
                    DOM::Value(static_cast<i64>(drawn.drawCount))
                );
                gpu.emplace(
                    "indirectDraws",
                    DOM::Value(static_cast<i64>(drawn.indirectDrawCount))
                );
                gpu.emplace(
                    "triangles",
                    DOM::Value(static_cast<i64>(gpuFrame->triangles))
                );
            }
            result.emplace("gpu", DOM::Value(std::move(gpu)));

            return DOM::Value(std::move(result));
        }
    }

    CROWY_STRUCT(RenderDebug)
        .SetProperty("mode", &RenderDebug::mode)
        .SetProperty("wireframe", &RenderDebug::wireframe)
        .SetProperty("depthPrepass", &RenderDebug::depthPrepass)
        .SetProperty("shadowFilter", &RenderDebug::shadowFilter)
        .SetProperty("showStats", &RenderDebug::showStats)
        .SetProperty("showPanel", &RenderDebug::showPanel)
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
        pipelineConfig = standardConfig(config, debug);
        pipeline = describePipeline();
        targetCaptures = std::make_unique<TargetCaptureQueue>(device);
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

        ApplyProperties(&debug, parseJsonString(env));

        // a misspelled key applies nothing, so the log says what took effect
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
                DOM::Value(
                    static_cast<i64>(
                        swapchain->PendingFrameDumps() +
                        targetCaptures->Pending()
                    )
                )
            );
            result.emplace(
                "captureFailures",
                DOM::Value(static_cast<i64>(captureFailures))
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
        // answers at once with what it queued; ping's capturesPending says
        // when the files are on disk, and captureFailures whether one failed
        port->RegisterVerb(
            "capture_frame",
            [this](const DOM::Value& args, Reply reply) {
                const auto path = args.get<Str>("path");
                if(!path || path->empty()) {
                    reply.Error("\"path\" is missing or not a string");

                    return;
                }
                // a target of the running pipeline by name; none is the back
                // buffer
                Str target;
                if(args.at("target") != nullptr) {
                    const auto named = args.get<Str>("target");
                    if(!named || named->empty()) {
                        reply.Error("\"target\" is not a string");

                        return;
                    }
                    if(auto refusal = refuseCaptureTarget(*named);
                       !refusal.empty()) {
                        reply.Error(std::move(refusal));

                        return;
                    }
                    target = *named;
                }

                auto selector = parseFrameSelector(args);
                if(!selector.error.empty()) {
                    reply.Error(selector.error);

                    return;
                }
                if(selector.frames.empty())
                    selector.frames.push_back(FrameNumber() + 1);

                const auto& frames = selector.frames;
                if(frames.size() > 1 && !path->contains(FramePlaceholder)) {
                    reply.Error("path needs {frame} for more than one frame");

                    return;
                }

                std::vector<Str> paths;
                paths.reserve(frames.size());
                for(const auto frame: frames)
                    paths.push_back(withFrame(*path, frame));
                // all or none: every frame is checked before any is queued
                if(auto refusal = refuseCaptures(target, frames, paths); !refusal.empty()) {
                    reply.Error(std::move(refusal));

                    return;
                }
                queueCaptures(target, frames, paths);

                DOM::Array queuedFrames;
                DOM::Array queuedPaths;
                for(usize i = 0; i < frames.size(); ++i) {
                    queuedFrames.emplace_back(static_cast<i64>(frames[i]));
                    queuedPaths.emplace_back(paths[i]);
                }

                DOM::Table result;
                result.emplace("frames", DOM::Value(std::move(queuedFrames)));
                result.emplace("paths", DOM::Value(std::move(queuedPaths)));
                reply.Ok(DOM::Value(std::move(result)));
            }
        );
        // with no selector, the last frame to finish: verbs drain before the
        // next one records. With one, each frame it names, from the history
        port->RegisterVerb("read_stats", [this](const DOM::Value& args, Reply reply) {
            const auto selector = parseFrameSelector(args);
            if(!selector.error.empty()) {
                reply.Error(selector.error);

                return;
            }
            if(selector.frames.empty()) {
                const auto& last = LastFrameStats();
                reply.Ok(statsToDom(last, gpuFrameStats(last)));

                return;
            }

            DOM::Array frames;
            frames.reserve(selector.frames.size());
            for(const auto frame: selector.frames) {
                if(frame > FrameNumber()) {
                    reply.Error(std::format(
                        "frame {} has not ended (the last frame is {})",
                        frame,
                        FrameNumber()
                    ));

                    return;
                }

                const auto* stats = frameStats.Find(frame);
                if(stats == nullptr) {
                    reply.Error(std::format(
                        "frame {} is not in the history (it keeps frames {}..{})",
                        frame,
                        frameStats.Oldest(),
                        frameStats.Newest()
                    ));

                    return;
                }
                frames.push_back(statsToDom(*stats, gpuFrameStats(*stats)));
            }

            DOM::Table result;
            result.emplace("frames", DOM::Value(std::move(frames)));
            reply.Ok(DOM::Value(std::move(result)));
        });
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

    Str RenderApp::refuseCaptureTarget(StrView name) const {
        const auto id = pipeline->FindTarget(name);
        if(!id) {
            Str names;
            for(const auto& target: pipeline->Targets()) {
                if(!pipeline->FindTarget(target.name))
                    continue;
                if(!names.empty())
                    names += ", ";
                names += target.name;
            }

            return std::format(
                "unknown target '{}' (the pipeline has {})",
                name,
                names
            );
        }
        if(!isCapturable(pipeline->Targets()[*id - 1].format)) {
            return std::format(
                "target '{}' has a format a capture cannot convert",
                name
            );
        }

        return {};
    }

    Str RenderApp::RequestCapture(Str path, u64 frame) {
        const std::array frames{frame};
        const std::array paths{std::move(path)};
        if(auto refusal = refuseCaptures({}, frames, paths); !refusal.empty())
            return refusal;
        queueCaptures({}, frames, paths);

        return {};
    }

    Str RenderApp::refuseCaptures(StrView target, std::span<const u64> frames, std::span<const Str> paths) const {
        for(const auto frame: frames) {
            if(frame <= FrameNumber())
                return std::format("frame {} has already been submitted (the last frame is {})", frame, FrameNumber());
        }

        const usize pending = target.empty() ? swapchain->PendingFrameDumps() : targetCaptures->Pending();
        if(pending + frames.size() > MaxFrameDumps)
            return std::format("the capture queue is full ({} pending)", pending);
        for(usize i = 0; i < frames.size(); ++i) {
            if(!isCaptureQueued(frames[i], target, paths[i]))
                continue;

            const auto what = target.empty()
                ? std::format("frame {}", frames[i])
                : std::format("'{}' at frame {}", target, frames[i]);

            return std::format("a capture for {} or to '{}' is already queued", what, paths[i]);
        }

        return {};
    }

    void RenderApp::queueCaptures(StrView target, std::span<const u64> frames, std::span<const Str> paths) {
        for(usize i = 0; i < frames.size(); ++i) {
            if(target.empty()) {
                swapchain->RequestFrameDump(paths[i], frames[i]);
            } else {
                targetCaptures->Request(TargetCaptureRequest{.frame = frames[i], .target = Str(target), .path = paths[i]});
            }
        }
    }

    bool RenderApp::isCaptureQueued(
        u64 frame,
        StrView target,
        StrView path
    ) const {
        // no request is for frame 0, so that asks the swapchain about the
        // path alone
        if(target.empty()) {
            return swapchain->IsFrameDumpQueued(frame, path) ||
                   targetCaptures->IsQueued(frame, {}, path);
        }

        return targetCaptures->IsQueued(frame, target, path) ||
               swapchain->IsFrameDumpQueued(0, path);
    }

    // a request whose target the list lost in a rebuild fails; a second one
    // for a target already taken this frame waits for the next
    std::span<TargetReadback> RenderApp::takeDueCaptures() {
        frameCaptures.clear();
        for(auto& request: targetCaptures->TakeDue(FrameNumber())) {
            const auto taken = std::ranges::any_of(
                frameCaptures,
                [&request](const TargetReadback& readback) {
                    return readback.request.target == request.target;
                }
            );
            if(taken) {
                targetCaptures->Request(std::move(request));
                continue;
            }

            const auto id = pipeline->FindTarget(request.target);
            if(!id) {
                targetCaptures->Fail(
                    request,
                    "the pipeline has no such target"
                );
                continue;
            }
            if(!isCapturable(pipeline->Targets()[*id - 1].format)) {
                targetCaptures->Fail(request, "its format cannot be converted");
                continue;
            }
            frameCaptures.push_back(
                TargetReadback{.request = std::move(request), .target = *id}
            );
        }

        return frameCaptures;
    }

    // a capture's reply went out when it was queued, so a failure reaches
    // the client through ping and the log
    void RenderApp::collectCaptures() {
        for(const auto& outcome: swapchain->TakeFrameDumpOutcomes()) {
            if(outcome.written)
                continue;

            ++captureFailures;
            LOG_WARN(
                "RenderApp",
                "capture of frame {} (presented {}) to '{}' failed",
                outcome.requested,
                outcome.presented,
                outcome.path
            );
        }

        // the fence is read live, so a held loop still collects; the queue
        // logs each failure with its target
        targetCaptures->Collect(device->GetCompletedFrame());
        for(const auto& outcome: targetCaptures->TakeOutcomes()) {
            if(!outcome.written)
                ++captureFailures;
        }
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
        collectCaptures();
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
            recorded.visiblePrimitives,
            scene.Primitives().Count(),
            recorded.draws,
            recorded.runs,
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

        auto& view = renderer->View(ViewMain);
        view.viewProj = camera->ViewProj(aspect);
        view.debugMode = static_cast<u32>(frameDebug.mode);
        view.shadowFilter = static_cast<u32>(frameDebug.shadowFilter);
        view.cameraPosition = toVec4(camera->Position(), 1.0f);

        // the prepass and the post list are passes in the list, so turning
        // either over is a new list; frames in flight keep reading the old
        // walker's targets
        auto wanted = standardConfig(config, frameDebug);
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

        // every per-frame buffer settles before the first pass opens
        OnUpdateFrameData();

        renderer->BeginFrame(scene, pipeline->ShadowMapSize());
        pipeline->Prepare(*renderer, scene, overrides);
        renderer->Upload();

        recorded = FrameStats{};
        recorded.visiblePrimitives = renderer->Visible(ViewMain).primitiveCount;

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
        frameInputs.captures = takeDueCaptures();

        pipeline->Record(cmdList, *renderer, frameInputs);

        for(auto& readback: frameCaptures)
            targetCaptures->AddInFlight(std::move(readback));
        frameCaptures.clear();
        frameInputs.captures = {};
        frameInputs.hooks = {};
        frameHooks.clear();

        // the edges and a hook pass's draws are counted as they are recorded
        const auto passStats = pipeline->Stats();
        recorded.passes.assign(passStats.begin(), passStats.end());
        for(const auto& pass: passStats) {
            recorded.triangles += pass.triangles;
            recorded.draws += pass.draws;
            recorded.runs += pass.runs;
        }
        reportCullStatsOnce();
    }

    void RenderApp::OnFrameEnd(const FrameReport& report) {
        CROWY_ASSERT(report.frame == FrameNumber(),
            "the profiler and the app count different frames"
        );

        // keyed by the app's own number, which every build keeps
        auto stats = recorded;
        stats.report = report;
        stats.primitives = scene.Primitives().Count();
        stats.pipelines = renderer->PipelineCount();
        frameStats.Push(FrameNumber(), stats);
    }

    const RenderApp::FrameStats* RenderApp::gpuFrameStats(
        const FrameStats& stats
    ) const noexcept {
        return stats.report.gpu ? frameStats.Find(stats.report.gpu->frame)
                                : nullptr;
    }

    const RenderApp::FrameStats& RenderApp::LastFrameStats() const noexcept {
        static const FrameStats none;
        const auto* newest = frameStats.Find(frameStats.Newest());

        return newest != nullptr ? *newest : none;
    }

    void RenderApp::OnResize(u32 width, u32 height) {
        pipeline->Resize(width, height);
        aspect = static_cast<f32>(width) / height;
    }
}
