#include "RenderApp.hpp"

#include <utility>

namespace Crowy
{
    RenderApp::~RenderApp() = default;

    RenderApp::RenderApp(const Config& config, CameraRAII camera)
        : host(*this, config, std::move(camera)) {}

    void RenderApp::OnInit(RHIDevice& device, RHISwapchain& swapchain) {
        host.Init(
            device,
            swapchain,
            FrameLoop{
                .control = Control(),
                .frame = [this] { return FrameNumber(); },
                .elapsedSeconds = [this] { return ElapsedSeconds(); },
                .requestQuit = [this] { RequestQuit(); },
                .title = Runtime().window.title
            },
            Runtime().hold
        );
    }

    void RenderApp::NewFrame() {
        host.NewFrame();
    }

    void RenderApp::ProcessInput(const InputProvider& input) {
        host.ProcessInput(input);
    }

    void RenderApp::OnUpdate(f64 deltaTime, f64) {
        host.Update(deltaTime);
    }

    void RenderApp::OnRecord(
        RHICommandList& cmdList,
        const RHIColorAttachment& backBuffer
    ) {
        host.Record(cmdList, backBuffer);
    }

    void RenderApp::OnFrameEnd(const FrameReport& report) {
        host.FrameEnd(report);
    }

    void RenderApp::OnResize(u32 width, u32 height) {
        host.Resize(width, height);
    }
}
