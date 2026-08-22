#include "RenderApp.hpp"

#include <array>

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

    RenderApp::RenderApp(const Config& config)
        : config(config), camera(config.camera) {}

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
            config.drawCapacity,
            config.viewCount
        );

        OnCreatePipelines(device, swapchain.GetFormat(), config.depthFormat);
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
        camera.ProcessInput(input);
    }

    void RenderApp::OnUpdate(f64 deltaTime, f64) {
        camera.Update(deltaTime);
    }

    void RenderApp::OnBindPass(RHICommandList& cmdList, u64 drawDataID) {
        cmdList.SetPushGraphicsConstants(ScenePush{.draws = drawDataID});
    }

    // does not prove the culling did anything.
    void RenderApp::reportCullStatsOnce() {
        if(reportedCullStats)
            return;

        reportedCullStats = true;
        LOG_INFO(
            "RenderApp",
            "first frame: {} of {} primitives survived culling",
            renderer->DrawCount(),
            scene.PrimitiveCount()
        );
    }

    void RenderApp::OnRecord(
        RHICommandList& cmdList,
        const RHIColorAttachment& backBuffer
    ) {
        renderer->View(ViewMain).viewProj = camera.ViewProj(aspect);

        // every per-frame buffer settles before the pass opens
        renderer->BuildFrame(scene, ViewMain);
        renderer->Upload();
        reportCullStatsOnce();

        auto colorAttachment = backBuffer;
        colorAttachment.clearColor = config.clearColor;
        std::array colorAttachments = {colorAttachment};
        const std::array acquires{
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

        cmdList.SetVertexBuffer(
            geometryPool->GetVertexBuffer(),
            0,
            sizeof(Vertex)
        );
        renderer->BindView(cmdList, ViewCBSlot, ViewMain);
        OnBindPass(cmdList, renderer->DrawDataID());

        renderer->Submit(
            cmdList,
            Pipeline(),
            RHIIndexBufferView{.buffer = &geometryPool->GetIndexBuffer()}
        );

        const std::array releases{ReleaseBackBuffer(backBuffer)};
        cmdList.EndRenderPass(releases);
    }

    void RenderApp::OnResize(u32 width, u32 height) {
        createDepthBuffer(width, height);
        aspect = static_cast<f32>(width) / height;
    }
}
