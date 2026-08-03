#pragma once

#include <print>
#include "CommandListPool.hpp"
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
        Timer timer;

    public:
        virtual ~App() = default;

        virtual void OnInitialRecord(RHICommandList&){};
        void RenderOnce(CommandListPool& pool) override final;

        virtual void OnUpdate(f64 deltaTime, f64 elapsedTime){}
        bool Update() override final{
            timer.NewFrame();

            OnUpdate(
                timer.GetDeltaTime(),
                timer.GetElapsedTime()
            );
            return true;
        }

        virtual void OnRecord(RHICommandList&, const RHIColorAttachment& backBuffer) = 0;
        void Render(CommandListPool& pool, RHISwapchain& swapchain) override final;
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
    int Main(const WindowConfig& windowConfig){
        RuntimeConfig runtimeConfig{
            .window = windowConfig
        };

        try{
            auto device = CreateDevice();

            OS os(runtimeConfig, *device);
            T app;

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
}

