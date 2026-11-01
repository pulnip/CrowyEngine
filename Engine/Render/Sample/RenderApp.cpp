#include "RenderApp.hpp"

#include <utility>

namespace Crowy
{
    RenderApp::~RenderApp() = default;

    RenderApp::RenderApp(const Config& config, CameraRAII camera)
        : host(*this, config, std::move(camera)) {}

    void RenderApp::init(RHIDevice& device, RHISwapchain& swapchain) {
        auto& clock = OS::Get()->Clock();
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
                .control = clock.Control(),
                .frame = [&clock] { return clock.Frame(); },
                .elapsedSeconds = [&clock] { return clock.ElapsedTime(); },
                .requestQuit = [&clock] { clock.RequestQuit(); },
                .title = Runtime().window.title,
                .input = &OS::Get()->GetInputProvider(),
                .inputStatus = [] { return OS::Get()->ReadInput(); },
                .pushInput = std::move(pushInput)
            },
            Runtime().hold
        );
    }

    void RenderApp::newFrame() {
        host.NewFrame();
    }

    void RenderApp::processInput(const InputProvider& input) {
        host.ProcessInput(input);
    }

    void RenderApp::update(f64 deltaTime, f64) {
        host.Update(deltaTime);
    }

    bool RenderApp::wantsRelativeMouse() {
        return host.WantsRelativeMouse();
    }

    void RenderApp::record(
        RHICommandList& cmdList,
        const RHIColorAttachment& backBuffer
    ) {
        host.Record(cmdList, backBuffer);
    }

    void RenderApp::endFrame(const FrameReport& report) {
        host.FrameEnd(report);
    }

    void RenderApp::resize(u32 width, u32 height) {
        host.Resize(width, height);
    }
}
