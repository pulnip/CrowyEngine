#include <format>
#include <stdexcept>
#include <SDL3/SDL_init.h>
#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>
#ifdef __APPLE__
    #include <SDL3/SDL_metal.h>
#endif
#include "FramePacer.hpp"
#include "Input.hpp"
#include "SDLInputProvider.hpp"
#include "Logger.hpp"
#include "MainLoop.hpp"
#include "OS.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"
#include "SDLTimer.hpp"

namespace Crowy
{
    OS* OS::instance = nullptr;

    class OS::Impl{
    private:
        SDL_Window* window = nullptr;
        int width, height;
    #ifdef __APPLE__
        SDL_MetalView view;
    #endif

        SDLTimer timer;

        MainLoop* mainLoop = nullptr;
        bool forceQuit = false;
        int exitCode = 0;

        std::unique_ptr<InputProvider> inputProvider;

        RHIDevicePtr device;
        RHISwapchainPtr swapchain;
        RHICommandListPtr cmdList;
        FramePacerPtr framePacer;

    public:
        Impl(const WindowConfig& config)
            :window(SDL_CreateWindow(config.title,
                config.width, config.height,
                (config.fullscreen    ? SDL_WINDOW_FULLSCREEN    : 0) |
                (config.resizable     ? SDL_WINDOW_RESIZABLE     : 0) |
                (config.borderless    ? SDL_WINDOW_BORDERLESS    : 0) |
                (config.always_on_top ? SDL_WINDOW_ALWAYS_ON_TOP : 0)
            ))
            ,width(config.width), height(config.height)
            ,device(createDevice())
            ,inputProvider(std::make_unique<SDLInputProvider>())
        {
            if(window == nullptr){
                throw std::runtime_error(std::format(
                    "Couldn't create window: {}",
                    SDL_GetError()
                ));
            }
        #ifdef __APPLE__
            view = SDL_Metal_CreateView(window);
        #endif

            if(device == nullptr)
                throw std::runtime_error("Couldn't create device");
            if(inputProvider == nullptr)
                throw std::runtime_error("Couldn't create input provider");

            swapchain = device->createSwapchain(RHISwapchainCreateDesc{
            #ifdef __APPLE__
                .windowHandle = SDL_Metal_GetLayer(view),
            #endif
                .width  = static_cast<uint32_t>(config.width),
                .height = static_cast<uint32_t>(config.height),
                .format = RHITextureFormat::RGBA8_UNORM,
                // triple buffering
                .bufferCount = 3,
                .vsync = true,
                .allowTearing = false,
                .debugName = "RHISwapchain"
            });
            cmdList = device->createCommandList();
            framePacer = device->createFramePacer();
        }

        ~Impl(){
            if(window != nullptr){
                SDL_DestroyWindow(window);
            }
        }

        void run(){
            if(!mainLoop) return;

            forceQuit = false;
            mainLoop->initialize();

            timer.reset();

            while(!forceQuit){
                processEvents();

                timer.newFrame();
                auto deltaTime = timer.getScaledDeltaTime();
                auto totalTime = timer.getTotalTime();

                if(!framePacer->beginFrame())
                    continue;
                if(!swapchain->acquireNextImage()){
                    framePacer->endFrame();
                    continue;
                }

                cmdList->reset();
                cmdList->begin();

                if(!mainLoop->update(deltaTime, totalTime))
                    break;

                cmdList->signalFence(
                    framePacer->getCurrentFence(),
                    framePacer->getNextFenceValue()
                );
                cmdList->close();
                device->submit(cmdList.get(), swapchain.get());

                framePacer->endFrame();
            }

            framePacer->waitForIdle();

            mainLoop->finalize();
        }

        void processEvents(){
            SDL_Event event;
            while(SDL_PollEvent(&event)){
                switch(event.type){
                case SDL_EVENT_QUIT:
                    forceQuit = true;
                    break;
                case SDL_EVENT_KEY_DOWN:
                    [[fallthrough]];
                case SDL_EVENT_KEY_UP:
                    pollInput();
                }
            }
        }

        int getWidth   () const{ return width;    }
        int getHeight  () const{ return height;   }
        int getExitCode() const{ return exitCode; }

        InputProvider*  getInputProvider(){ return inputProvider.get(); }
        RHIDevice*      getDevice       (){ return        device.get(); }
        RHISwapchain*   getSwapchain    (){ return     swapchain.get(); }
        RHICommandList* getCommandList  (){ return       cmdList.get(); }

        void setMainLoop(MainLoop* mainLoop){
            this->mainLoop = mainLoop;
        }
    };

    OS::OS(const WindowConfig& config){
        if(!SDL_SetAppMetadata("Crowy", "1.0", "io.github.pulnip.crowy")){
            throw std::runtime_error(std::format(
                "Couldn't set app metadata: {}",
                SDL_GetError()
            ));
        }
        if(!SDL_Init(SDL_INIT_EVENTS | SDL_INIT_VIDEO)){
            throw std::runtime_error(std::format(
                "Couldn't initialize SDL: {}",
                SDL_GetError()
            ));
        }

        impl = std::make_unique<Impl>(config);

        instance = this;
    }

    OS::~OS(){ instance = nullptr; }

    void OS::run(){ impl->run(); }
    void OS::processEvents(){ impl->processEvents(); }

    int      OS::getWidth   () const{ return impl->getWidth();    }
    int      OS::getHeight  () const{ return impl->getHeight();   }
    int      OS::getExitCode() const{ return impl->getExitCode(); }

    InputProvider*  OS::getInputProvider(){ return impl->getInputProvider(); }
    RHIDevice*      OS::getDevice       (){ return impl->getDevice();        }
    RHISwapchain*   OS::getSwapchain    (){ return impl->getSwapchain();     }
    RHICommandList* OS::getCommandList  (){ return impl->getCommandList();   }

    void OS::setMainLoop(MainLoop* mainLoop){ impl->setMainLoop(mainLoop); }
}