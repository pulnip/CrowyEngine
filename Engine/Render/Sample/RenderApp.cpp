#include "RenderApp.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
#include <format>
#include <stdexcept>
#include <utility>
#include <vector>

#include "Log.hpp"
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
        // a frame number or a frame count on the wire: an integer from 1 up
        std::optional<u64> positiveInteger(const DOM::Value* value) {
            if(value == nullptr)
                return std::nullopt;

            const auto integer = value->asInt();
            if(!integer || *integer < 1)
                return std::nullopt;

            return static_cast<u64>(*integer);
        }
    }

    RenderApp::~RenderApp() = default;

    RenderApp::RenderApp(const Config& config, CameraRAII camera)
        : config(config), camera(std::move(camera)) {}

    void RenderApp::createDepthBuffer(u32 width, u32 height) {
        depthBuffer = device->CreateTexture(
            RHITextureCreateDesc{
                .width = width,
                .height = height,
                .format = config.depthFormat,
                .usage = RHITextureUsage::DepthStencil,
                .clearDepthStencil = {.depth = 1.0f}
            }
        );
    }

    void RenderApp::OnInit(RHIDevice& device, RHISwapchain& swapchain) {
        this->device = &device;
        this->swapchain = &swapchain;

        createDepthBuffer(swapchain.GetWidth(), swapchain.GetHeight());
        aspect = static_cast<f32>(swapchain.GetWidth()) / swapchain.GetHeight();

        geometryPool = std::make_unique<GeometryPool>(
            device,
            config.vertexPoolCapacity,
            config.indexPoolCapacity
        );
        renderer = std::make_unique<SceneRenderer>(
            device,
            SceneRendererDesc{
                .drawCapacity = config.drawCapacity,
                .materialCapacity = config.materialCapacity,
                .viewCount = config.viewCount
            }
        );

        colorFormat = swapchain.GetFormat();

        OnInitUI(device, colorFormat, config.depthFormat);

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
        // a port that could not bind stays, inert, so a sample can show
        // that it did not; only its verbs are skipped
        if(port->Port() == 0)
            return;

        port->RegisterVerb("ping", [this](const DOM::Value&, Reply reply) {
            auto result = controlStatus();
            result.emplace("pong", DOM::Value(true));
            result.emplace("app", DOM::Value(Runtime().window.title));
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
                const auto count = positiveInteger(frames);
                if(!count) {
                    reply.Error("frames must be a positive integer");

                    return;
                }
                Control().RunFrames(*count);
            } else if(until != nullptr) {
                const auto frame = positiveInteger(until);
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
                const auto frame = positiveInteger(args.at("frame"));
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
        port->RegisterVerb(
            "capture_frame",
            [this](const DOM::Value& args, Reply reply) {
                const auto path = args.get<Str>("path");
                if(!path || path->empty()) {
                    reply.Error("\"path\" is missing or not a string");

                    return;
                }
                if(pendingCapture.has_value()) {
                    reply.Error("a capture is already pending");

                    return;
                }
                // the boot-time CROWY_DUMP_FRAME request occupies the same slot
                if(!swapchain->RequestFrameDump(*path)) {
                    reply.Error("a frame dump is already pending");

                    return;
                }

                pendingCapturePath = *path;
                pendingCapture = std::move(reply);
            }
        );
        // every number describes `frame`, the last frame to finish: verbs
        // drain before the next one records
        port->RegisterVerb("read_stats", [this](const DOM::Value&, Reply reply) {
            constexpr auto MsPerSecond = 1000.0;
            const auto& stats = frameStats;

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
                "buckets",
                DOM::Value(static_cast<i64>(stats.buckets))
            );
            result.emplace(
                "pipelines",
                DOM::Value(static_cast<i64>(stats.pipelines))
            );
            result.emplace("instrumented", DOM::Value(static_cast<bool>(CROWY_FRAME_STATS)));

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
            reply.Ok(DOM::Value(std::move(result)));
        });
    }

    void RenderApp::pollCapture() {
        if(!pendingCapture.has_value())
            return;

        switch(swapchain->GetFrameDumpState()) {
        case FrameDumpState::Written: {
            DOM::Table result;
            result.emplace("path", DOM::Value(pendingCapturePath));
            pendingCapture->Ok(DOM::Value(std::move(result)));
            pendingCapture.reset();
            break;
        }
        case FrameDumpState::Failed:
            pendingCapture->Error("frame dump failed; see the log");
            pendingCapture.reset();
            break;
        case FrameDumpState::Idle:
            [[fallthrough]];
        case FrameDumpState::Pending:
            break;
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
        pollCapture();
        answerWaits();
        port->Drain();
    }

    void RenderApp::ProcessInput(const InputProvider& input) {
        camera->ProcessInput(input);
        OnProcessInput(input);
    }

    void RenderApp::OnUpdate(f64 deltaTime, f64) {
        camera->Update(deltaTime);
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
            "{} draws in {} buckets over {} pipelines",
            renderer->VisiblePrimitiveCount(),
            scene.Primitives().Count(),
            renderer->DrawCount(),
            renderer->BucketCount(),
            renderer->PipelineCount()
        );
    }

    void RenderApp::OnRecord(
        RHICommandList& cmdList,
        const RHIColorAttachment& backBuffer
    ) {
        renderer->View(ViewMain).viewProj = camera->ViewProj(aspect);

        // every per-frame buffer settles before the pass opens
        OnUpdateFrameData();
        const std::array passFormats = {colorFormat};
        renderer->BuildFrame(
            scene,
            PassPipelineDesc{
                .renderTargetFormats = passFormats,
                .depthFormat = config.depthFormat
            },
            ViewMain
        );
        renderer->Upload();
        reportCullStatsOnce();

        // the scene pass is the pool's first reader, so it takes the releases
        const auto geometryAcquires = geometryPool->RecordUploads(cmdList);
        const auto uiAcquires = OnPrepareUI(cmdList);

        auto colorAttachment = backBuffer;
        colorAttachment.clearColor = config.clearColor;
        std::array colorAttachments = {colorAttachment};
        std::vector<RHITextureBarrier> acquires{
            AcquireBackBuffer(backBuffer),
            // waits for the previous frame's depth work (WAR),
            // contents discarded - the pass clears anyway
            MakeCrossSubmissionBarrier(
                *depthBuffer,
                RHIResourceUsage::DepthWrite,
                RHIResourceUsage::DepthWrite,
                /*discardContents=*/true
            )
        };
        acquires.append_range(uiAcquires);
        cmdList.BeginRenderPass(
            RHIRenderPassDesc{
                .colorAttachments = colorAttachments,
                .depthAttachment =
                    RHIDepthAttachment{
                        .texture = depthBuffer.get(),
                        .loadAction = RHILoadAction::Clear,
                        .storeAction = RHIStoreAction::DontCare,
                        .clearDepthStencil = {.depth = 1.0f}
                    }
            },
            acquires,
            geometryAcquires
        );
        cmdList.SetViewport(FullViewport(*backBuffer.texture));
        cmdList.SetScissorRect(FullScissorRect(*backBuffer.texture));

        renderer->BindView(cmdList, ViewCBSlot, ViewMain);
        auto push = renderer->Push();
        // the pool is GPUOnly, so unlike the renderer's own buffers this one
        // does not have to be re-resolved
        push.vertices = geometryPool->GetVertexBufferID();
        OnBindPass(cmdList, push);

        renderer->Submit(
            cmdList,
            RHIIndexBufferView{.buffer = &geometryPool->GetIndexBuffer()}
        );

        OnRecordUI(cmdList);

        const std::array releases{ReleaseBackBuffer(backBuffer)};
        cmdList.EndRenderPass(releases);
    }

    void RenderApp::OnFrameEnd(const FrameReport& report) {
        CROWY_ASSERT(report.frame == FrameNumber(),
            "the profiler and the app count different frames"
        );

        frameStats = FrameStats{
            .report = report,
            .primitives = scene.Primitives().Count(),
            .visiblePrimitives = renderer->VisiblePrimitiveCount(),
            .triangles = renderer->TriangleCount(),
            .draws = renderer->DrawCount(),
            .buckets = renderer->BucketCount(),
            .pipelines = renderer->PipelineCount()
        };
    }

    void RenderApp::OnResize(u32 width, u32 height) {
        createDepthBuffer(width, height);
        aspect = static_cast<f32>(width) / height;
    }
}
