#pragma once

#include <print>
#include <comdef.h>
#include "DX12CommandListPool.hpp"
#include "DX12Device.hpp"
#include "DX12FramePacer.hpp"
#include "RHIDefinitions.hpp"
#include "RuntimeConfig.hpp"
#include "SDLWindow.hpp"
#include "Timer.hpp"

namespace Crowy
{
    class Sample{
    protected:
        DX12Device device{};
        DX12FramePacer framePacer{device};
        DX12CommandListPool cmdListPool{device};
        SDLWindow window;
        RAII<DX12Swapchain> swapchain;
        Timer timer;

    public:
        explicit Sample(const WindowConfig&);
        virtual ~Sample() = default;

        virtual void OnInit(DX12Device&) = 0;
        virtual void OnUpdate(f64 deltaTime, f64 elapsedTime){}
        virtual void OnRecord(DX12CommandList&, const RHIColorAttachment& backBuffer) = 0;

        int Run();

    private:
        bool pumpEvents();
        void renderFrame();
    };

    RHIViewport FullViewport(const RHITexture&);
    RHIScissorRect FullScissorRect(const RHITexture&);

    template<std::derived_from<Sample> T>
    int Main(const WindowConfig& config){
        try{
            T sample(config);

            sample.Run();
        }
        catch(const _com_error& e){
            std::println(
                "COM Exception: {} (hr=0x{:08X})",
                static_cast<CStr>(e.ErrorMessage()), e.Error()
            );

            return 1;
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

