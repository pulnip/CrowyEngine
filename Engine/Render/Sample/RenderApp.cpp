#include "RenderApp.hpp"

#include <array>
#include <charconv>
#include <cstdlib>
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
        if(port->Port() == 0) {
            port = nullptr;

            return;
        }

        port->RegisterVerb("ping", [this](const DOM::Value&, Reply reply) {
            DOM::Table result;
            result.emplace("pong", DOM::Value(true));
            result.emplace("app", DOM::Value(Runtime().window.title));
            result.emplace("frame", DOM::Value(static_cast<i64>(FrameNumber())));
            reply.Ok(DOM::Value(std::move(result)));
        });
        port->RegisterVerb("quit", [this](const DOM::Value&, Reply reply) {
            RequestQuit();
            reply.Ok(DOM::Value(DOM::Table{}));
        });
    }

    void RenderApp::NewFrame() {
        if(port != nullptr)
            port->Drain();
    }

    void RenderApp::OnInitialRecord(RHICommandList& cmdList) {
        const auto acquires = geometryPool->UploadAcquires();
        cmdList.BeginBlitPass({}, acquires);
        OnBuildGeometry(cmdList, *geometryPool);
        // the draws live in later submissions,
        // so these releases complete at Close
        // as the hand-off to vertex/index use
        const auto releases = geometryPool->UploadReleases();
        cmdList.EndBlitPass({}, releases);

        geometryPool->LogAllocationStats();

        // after the geometry, because a snapshot carries the allocation
        ExtractScene(scene);
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
            renderer->DrawCount(),
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
            acquires
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

    void RenderApp::OnResize(u32 width, u32 height) {
        createDepthBuffer(width, height);
        aspect = static_cast<f32>(width) / height;
    }
}
