#pragma once

#include <optional>

#include "FrameClock.hpp"
#include "FrameContent.hpp"
#include "FrameHost.hpp"
#include "Main.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "Primitives.hpp"
#include "RuntimeConfig.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    class RenderApp: public MainLoop, public FrameContent {
    public:
        using Config = FrameHost::Config;
        using FrameStats = Crowy::FrameStats;

        static constexpr u32 ViewMain = FrameHost::ViewMain;

    private:
        FrameHost host;

    public:
        ~RenderApp() override;
        CROWY_DECLARE_PINNED(RenderApp)

        RenderApp(const Config& config, CameraRAII camera);

    protected:
        const RuntimeConfig& Runtime() const { return OS::Get()->Config(); }
        // the pacer's: the one being recorded, or between frames the last
        // that ended
        u64 FrameNumber() const { return OS::Get()->Clock().Frame(); }

        // the lit views' clear, as a lighting key's sky, from the next frame;
        // the pipeline stays as it is
        void SetClearColor(Color color) noexcept { host.SetClearColor(color); }
        // the scene view's size from the next frame on; a zero side is
        // ignored, as a collapsed window's
        void SetSceneViewSize(u32 width, u32 height) noexcept {
            host.SetSceneViewSize(width, height);
        }
        // the scene view target for the UI to sample this frame; 0 without
        // debug.sceneView
        u64 SceneViewID() const { return host.SceneViewID(); }
        // the cursor hidden for this frame, as a look turning the camera
        // asks; asked again every frame it holds
        void RequestRelativeMouse() noexcept { host.RequestRelativeMouse(); }
        // the back buffer of `frame` to `path` as a BMP, with the
        // capture_frame verb's checks; why not, empty once queued
        Str RequestCapture(Str path, u64 frame) {
            return host.RequestCapture(std::move(path), frame);
        }

        auto& Device() noexcept { return host.Device(); }
        auto& Geometry() noexcept { return host.Geometry(); }
        auto& Meshes() noexcept { return host.Meshes(); }
        auto& Materials() noexcept { return host.Materials(); }
        auto& Scene() noexcept { return host.Scene(); }
        auto& Renderer() noexcept { return host.Renderer(); }
        const auto& Camera() const noexcept { return host.Camera(); }
        auto& Camera() noexcept { return host.Camera(); }
        RenderDebug& Debug() noexcept { return host.Debug(); }
        f32 Aspect() const noexcept { return host.Aspect(); }
        // for a sample that registers verbs of its own
        CommandPort* Port() noexcept { return host.Port(); }
        // the newest frame that ended; empty before frame 1 has
        const FrameStats& LastFrameStats() const noexcept {
            return host.LastFrameStats();
        }
        // the formats of the running list's pass naming `hook`; empty when
        // the list has none, as a data view's
        std::optional<HookPassFormats> FindHook(StrView hook) const noexcept {
            return host.FindHook(hook);
        }

    private:
        void init(RHIDevice& device, RHISwapchain& swapchain) override final;
        void newFrame() override final;
        void processInput(const InputProvider& input) override final;
        void update(f64 deltaTime, f64 elapsedTime) override final;
        bool wantsRelativeMouse() override final;
        void record(
            RHICommandList& cmdList,
            const RHIColorAttachment& backBuffer
        ) override final;
        void endFrame(const FrameReport& report) override final;
        void resize(u32 width, u32 height) override final;
    };
}
