#include <SDL3/SDL.h>
#include "FramePacer.hpp"
#include "Log.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"
#include "Resource.hpp"
#include "Timer.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    // Logger::instance().setMinLevel(LogLevel::Warn);

    auto window = SDL_CreateWindow("Triangle", 800, 600, 0);
#ifdef __APPLE__
    auto view = SDL_Metal_CreateView(window);
#endif

    auto device = createDevice();
    initResourceModule(device.get());

    auto swapchain = device->createSwapchain(
        RHISwapchainCreateDesc{
        #ifdef __APPLE__
            .windowHandle = SDL_Metal_GetLayer(view),
        #elif _WIN32
            .windowHandle = SDL_GetPointerProperty(
                SDL_GetWindowProperties(window),
                SDL_PROP_WINDOW_WIN32_HWND_POINTER,
                nullptr
            ),
        #endif
            .width = 800,
            .height = 600,
            .format = RHITextureFormat::BGRA8_UNORM,
            .bufferCount = 3,
            .vsync = true,
            .allowTearing = false,
            .debugName = "RHISwapchain"
        }
    );
    auto cmdList = device->createCommandList();
    auto framePacer = device->createFramePacer();

    auto [meshHandle, materialSetHandle] = getOrLoad(
        ModelRequest{
            .uri = "file:asset/Stelle/Stelle.pmx"
        }
    );
    auto mesh = get(meshHandle);
    auto materialSet = get(materialSetHandle);

    auto uniformBuffer = device->createBuffer({
        .size = sizeof(Mat4),
        .usage = RHIBufferUsage::ConstantBuffer,
        .stride = 0,
        .initialData = nullptr,
        .debugName = "MVP Uniform Buffer"
    });

    Renderer renderer(device.get());

    RenderSpec spec{
        .passes = {
            RenderPassSpec{
                .name = "default",
                .inputs = {

                },
                .targets = {

                },
                .depthTarget = "depth",
                .shader = ShaderSpec{
                #ifdef __APPLE__
                    .vsFilePath = "asset/Shaders/triangle.metal",
                    .vsFuncName = "vs_main",
                    .fsFilePath = "asset/Shaders/triangle.metal",
                    .fsFuncName = "fs_textured",
                #elif _WIN32
                    .vsFilePath = L"asset/Shaders/standard_vs.hlsl",
                    .vsFuncName = "vs_main",
                    .fsFilePath = L"asset/Shaders/standard_ps.hlsl",
                    .fsFuncName = "ps_textured",
                #endif
                },
                .renderType = "type0",
                .rasterizer = RHIRasterizerState{},
                .blend = RHIBlendState{}
            },
        },
        .renderTargets = {
            {
                "depth",
                RHITextureCreateDesc{
                    .width = 800,
                    .height = 600,
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::D32_FLOAT,
                    .usage = RHITextureUsage::DepthStencil,
                    .initialState = RHIResourceState::DepthStencilWrite,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "Depth Buffer"
                },
            }
        }
    };
    renderer.loadPasses(spec);

    float cameraDistance = 30.0f;

    Timer timer;
    timer.reset();

    bool isRunning = true;

    while(isRunning){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            switch(event.type){
            case SDL_EVENT_QUIT:
                isRunning = false;
            }
        }

        timer.newFrame();
        float dt = timer.deltaSeconds();
        float et = timer.elapsedSeconds();

        if(framePacer->beginFrame()){
            if(!swapchain->acquireNextImage()){
                framePacer->endFrame();
                continue;
            }

            if(cmdList){
                auto aspect = float(800)/600;
                auto model = rotateYMat(0.5f * et);

                auto camX = std::sin(0.3f * et) * cameraDistance;
                auto camZ = std::cos(0.3f * et) * cameraDistance;
                auto view = lookAt(
                    Vec3{camX, 10.0f, camZ},
                    Vec3{0.0f, 10.0f, 0.0f},
                    Vec3{0.0f,  1.0f, 0.0f}
                );

                float fovRad = 45.0f * 3.14159265f / 180.0f;
                auto proj = perspective(fovRad, aspect, 0.1f, cameraDistance * 4.0f);

                auto mvp = transpose(proj * view * model);
                uniformBuffer->update(mvp.data(), sizeof(Mat4));

                RenderContext ctx;

                cmdList->reset();
                cmdList->begin();

                renderer.render(*cmdList.get(), ctx, swapchain.get());

                // Signal fence for frame synchronization
                cmdList->signalFence(
                    framePacer->getCurrentFence(),
                    framePacer->getNextFenceValue()
                );

                cmdList->close();
            }
            device->submit(cmdList.get(), swapchain.get());

            framePacer->endFrame();
        }
    }

    framePacer->waitForIdle();

    deinitResourceModule();

    return 0;
}