#pragma once

#include <print>
#include "CommandListPool.hpp"
#include "FramePacer.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHISwapchain.hpp"
#include "RHITexture.hpp"
#include "RuntimeConfig.hpp"
#include "SDLWindow.hpp"
#include "Timer.hpp"

namespace Crowy
{
    class App{
    protected:
        RHIDeviceRAII device = CreateDevice();
        FramePacer framePacer{*device};
        CommandListPool cmdListPool{*device};
        SDLWindow window;
        RHISwapchainRAII swapchain;
        Timer timer;

    public:
        explicit App(const WindowConfig&);
        virtual ~App() = default;

        virtual void OnInit(RHIDevice&) = 0;
        virtual void OnUpdate(f64 deltaTime, f64 elapsedTime){}
        virtual void OnRecord(RHICommandList&, const RHIColorAttachment& backBuffer) = 0;

        int Run();

    private:
        bool pumpEvents();
        void renderFrame();
    };

    RHIViewport FullViewport(const RHITexture&);
    RHIScissorRect FullScissorRect(const RHITexture&);

    template<std::derived_from<App> T>
    int Main(const WindowConfig& config){
        try{
            T sample(config);

            sample.Run();
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

