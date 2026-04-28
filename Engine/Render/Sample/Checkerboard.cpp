#include <SDL3/SDL.h>
#include "Logger.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    Logger::instance().setMinLevel(LogLevel::Warn);

    uint32_t width = 800, height = 600;

    auto window = SDL_CreateWindow("Checkerboard", width, height, 0);
    auto device = createDevice();
#ifdef CROWY_METALRHI
    auto view = SDL_Metal_CreateView(window);
#endif

    RHITextureCreateDesc backBufferDesc{
        .width = width,
        .height = height,
        .format = RHIPixelFormat::BGRA8_UNORM
    };
    auto swapchain = device->createSwapchain(
        RHISwapchainCreateDesc{
        #ifdef CROWY_METALRHI
            .windowHandle = SDL_Metal_GetLayer(view),
        #elif CROWY_D3DRHI
            .windowHandle = SDL_GetPointerProperty(
                SDL_GetWindowProperties(window),
                SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                nullptr
            ),
        #endif
            .bufferDesc = backBufferDesc,
            // triple buffering
            .bufferCount = 3,
            .vsync = true,
            .allowTearing = false
        #if defined(_DEBUG) || !defined(NDEBUG)
            , .debugName = "Swapchain"
        #endif
        }
    );
    auto cmdList = device->createCommandList();

    Renderer renderer(*device);
    RenderSpec spec{
        .textures = {
            {
                "checkerboard",
                RHITextureCreateDesc{
                    .width = 256,
                    .height = 256,
                    .format = RHIPixelFormat::BGRA8_UNORM,
                    .usage = TEX_AllowShaderRW,
                    .initialState = RHIResourceState::UnorderedAccess
                }
            }
        },
        .samplers = {
            {"LINEAR_WRAP", LINEAR_WRAP_SAMPLER}
        },
        .renderPasses = {
            {
                .name = "render",
                .outputs = {"BackBuffer"},
                .pipelines = {
                    {
                        .fs = {
                            .textures = {
                                {.slot = "tex", .name = "checkerboard"}
                            },
                            .samplers = {
                                {.slot = "s", .name = "LINEAR_WRAP"}
                            }
                        },
                        .shader = ShaderSpec{
                        #ifdef CROWY_METALRHI
                            .vsFilePath = "asset/Shaders/fullscreen.metal",
                            .vsFuncName = "vs_fullscreen",
                            .fsFilePath = "asset/Shaders/fullscreen.metal",
                            .fsFuncName = "fs_bypass"
                        #elifdef CROWY_D3DRHI
                            .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                            .vsFuncName = "vs_fullscreen",
                            .fsFilePath = L"asset/Shaders/fullscreen.hlsl",
                            .fsFuncName = "fs_bypass"
                        #endif
                        }
                    }
                }
            }
        },
        .computePasses = {
            {
                .name = "Checkerboard",
                .cs = {
                    .textures = {
                        {.slot = "out", .name = "checkerboard"}
                    }
                },
                .shader = {
                #ifdef CROWY_METALRHI
                    .filePath = "asset/Shaders/checkerboard.metal",
                    .funcName = "cs_checkerboard",
                #elif CROWY_D3DRHI
                    .filePath = L"asset/Shaders/checkerboard.hlsl",
                    .funcName = "cs_checkerboard",
                #endif
                },
                .gridSize = {width, height, 1}
            }
        }
    };

    try{
        renderer.loadPasses(spec);
    }
    catch(const std::exception& e){
        std::println("{}", e.what());

        return 0;
    }

    bool isRunning = true;

    while(isRunning){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_EVENT_QUIT:
                isRunning = false;
                break;
            }
        }

        auto scope = device->createFrameScope();

        if(!swapchain->acquireNextImage())
            continue;

        cmdList->begin();

        renderer.render(*cmdList.get(), {}, swapchain.get());

        cmdList->close();
        device->submit(*cmdList.get(), swapchain.get());
    }

    return 0;
}