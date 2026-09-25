#pragma once

#include <chrono>
#include <print>
#include <ratio>
#include <span>
#include <utility>
#include "Assert.hpp"
#include "CommandListPool.hpp"
#include "FrameControl.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"
#include "RuntimeConfig.hpp"
#include "Timer.hpp"

namespace Crowy
{
    class App: public MainLoop{
    private:
        // what a frame let through by a counted run takes, whatever the
        // clock says, so two runs of the same steps agree. 1/60 s rounds
        // once, to the clock's tick
        static constexpr auto FixedStep = std::chrono::round<Timer::Duration>(
            std::chrono::duration<i64, std::ratio<1, 60>>(1)
        );

        Timer timer;
        FrameControl control;
        RHIDevice* device = nullptr;
        const RuntimeConfig* runtime = nullptr;
        // the pacer's frame: the one being recorded, or between frames the
        // last one that ended
        u64 frame = 0;
        // the gap before a frame - a hold, or OnInit before frame 1 - is not
        // that frame's time
        bool held = true;
        // let through by a counted run: this frame's time is the fixed step
        bool stepped = false;
        bool quitRequested = false;

    public:
        virtual ~App() = default;

        void BindDevice(RHIDevice& device) noexcept{ this->device = &device; }
        RHIDevice& Device() const noexcept{
            CROWY_ASSERT(device != nullptr);
            return *device;
        }
        void BindRuntime(const RuntimeConfig& runtime) noexcept{
            this->runtime = &runtime;
        }
        const RuntimeConfig& Runtime() const noexcept{
            CROWY_ASSERT(runtime != nullptr);
            return *runtime;
        }

        u64 FrameNumber() const noexcept{
            return frame;
        }
        void OnFrameBegin(u64 frame) override final{
            this->frame = frame;
        }
        bool ShouldAdvance() override final{
            // a quit leaves through Update(), so it releases any hold, even
            // one that arrives after it
            const auto gate = quitRequested ?
                FrameControl::Gate::Free :
                control.Advance(frame);
            if(gate == FrameControl::Gate::Hold){
                held = true;
                return false;
            }

            if(std::exchange(held, false))
                timer.Rebase();
            stepped = gate == FrameControl::Gate::Step;
            return true;
        }

        virtual void OnUpdate(f64 deltaTime, f64 elapsedTime){}
        bool Update() override final{
            if(stepped)
                timer.Step(FixedStep);
            else
                timer.NewFrame();

            OnUpdate(
                timer.GetDeltaTime(),
                timer.GetElapsedTime()
            );
            return !quitRequested;
        }

        virtual void OnRecord(RHICommandList&, const RHIColorAttachment& backBuffer) = 0;
        void Render(CommandListPool& pool, RHISwapchain& swapchain) override final;

    protected:
        // the frame finishes, then the loop leaves through Update()
        void RequestQuit() noexcept{
            quitRequested = true;
        }

        FrameControl& Control() noexcept{ return control; }
        const FrameControl& Control() const noexcept{ return control; }
        // what OnUpdate has been told so far: fixed steps plus wall-clock
        // frames, with no hold in it
        f64 ElapsedSeconds() const noexcept{ return timer.GetElapsedTime(); }
    };

    RHIViewport FullViewport(const RHITexture&, u32 mipLevel = 0);
    RHIScissorRect FullScissorRect(const RHITexture&, u32 mipLevel = 0);

    // self-contained backbuffer barriers for the pass drawing to it.
    // the acquire discards previous contents (fine while the pass clears);
    // a sample that loads them instead should acquire Present → RenderTarget.
    inline RHITextureBarrier AcquireBackBuffer(const RHIColorAttachment& backBuffer){
        CROWY_ASSERT(backBuffer.loadAction != RHILoadAction::Load,
            "discarding acquire but the pass loads previous contents"
        );
        return MakeBarrier(
            *backBuffer.texture,
            RHIResourceUsage::Undefined,
            RHIResourceUsage::RenderTarget
        );
    }
    // Present ordering is the swapchain's guarantee, so no pair needed
    inline RHITextureBarrier ReleaseBackBuffer(const RHIColorAttachment& backBuffer){
        return MakeBarrier(
            *backBuffer.texture,
            RHIResourceUsage::RenderTarget,
            RHIResourceUsage::Present
        );
    }

    template<std::derived_from<App> T>
    int Main(const RuntimeConfig& runtimeConfig){
        try{
            auto device = CreateDevice();

            OS os(runtimeConfig, *device);
            T app;
            app.BindDevice(*device);
            app.BindRuntime(runtimeConfig);

            os.Run(app, *device);
        }
        catch(const std::exception& e){
            std::println("Exception: {}", e.what());

            return 1;
        }
        catch(...){
            std::println("Unhandled Exception");

            return 1;
        }

        return 0;
    }

    // a sample that only cares about its window
    template<std::derived_from<App> T>
    int Main(const WindowConfig& windowConfig){
        return Main<T>(RuntimeConfig{
            .window = windowConfig
        });
    }

    // a sample a run can configure from the command line (--config)
    template<std::derived_from<App> T>
    int Main(int argc, char** argv, const WindowConfig& windowConfig){
        RuntimeConfig runtimeConfig{
            .window = windowConfig
        };

        try{
            const auto args =
                std::span<char* const>(argv, static_cast<usize>(argc));
            applyCommandLine(
                runtimeConfig,
                args.empty() ? args : args.subspan(1)
            );
        }
        catch(const std::exception& e){
            std::println("{}", e.what());

            return 1;
        }

        return Main<T>(runtimeConfig);
    }
}

