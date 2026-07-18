#pragma once

#include <print>
#include "CommandListPool.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
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

        virtual void OnUpdate(f64 deltaTime, f64 elapsedTime){}
        virtual void OnRecord(RHICommandList&, const RHIColorAttachment& backBuffer) = 0;

        bool Update() override final{
            timer.NewFrame();

            OnUpdate(
                timer.GetDeltaTime(),
                timer.GetElapsedTime()
            );
            return true;
        }

        bool Render(CommandListPool& pool, RHISwapchain& swapchain) override{
            auto& cmdList = pool.Acquire();
            cmdList.Begin();
            cmdList.TransitionBarrier(
                swapchain.GetCurrentTexture(),
                RHIResourceUsage::RenderTarget
            );
            RHIColorAttachment backBuffer{
                .texture = &swapchain.GetCurrentTexture(),
                .loadAction = RHILoadAction::Clear,
                .storeAction = RHIStoreAction::Store,
                .clearColor = Colors::Black
            };

            OnRecord(cmdList, backBuffer);

            cmdList.TransitionBarrier(
                swapchain.GetCurrentTexture(),
                RHIResourceUsage::Present
            );
            cmdList.Close();

            return true;
        }
    };

    RHIViewport FullViewport(const RHITexture&);
    RHIScissorRect FullScissorRect(const RHITexture&);

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

