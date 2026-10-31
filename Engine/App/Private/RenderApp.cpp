#include "RenderApp.hpp"

#include <utility>

namespace Crowy
{
    RenderApp::~RenderApp() = default;

    RenderApp::RenderApp(const Config& config, CameraRAII camera)
        : host(*this, config, std::move(camera)) {}

    void RenderApp::OnInit(RHIDevice& device, RHISwapchain& swapchain) {
        std::function<void(const InputEvent&)> pushInput;
        if(Runtime().portInput) {
            pushInput = [](const InputEvent& input) {
                OS::Get()->PushInput(input);
            };
        }

        host.Init(
            device,
            swapchain,
            FrameLoop{
                .control = Control(),
                .frame = [this] { return FrameNumber(); },
                .elapsedSeconds = [this] { return ElapsedSeconds(); },
                .requestQuit = [this] { RequestQuit(); },
                .title = Runtime().window.title,
                .input = &OS::Get()->GetInputProvider(),
                .inputStatus = [] { return OS::Get()->ReadInput(); },
                .pushInput = std::move(pushInput)
            },
            Runtime().hold
        );
    }

    RenderSession& RenderApp::OnOpenSession(
        RHIDevice& device,
        GeometryPool& pool,
        const AssetRoots& roots
    ) {
        ownSession = std::make_unique<RenderSession>(device, pool, roots);
        return *ownSession;
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

    bool RenderApp::WantsRelativeMouse() {
        return host.WantsRelativeMouse();
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
