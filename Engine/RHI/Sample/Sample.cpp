#include <SDL3/SDL.h>
#include "FramePacer.hpp"
#include "Logger.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Resource.hpp"
#include "Timer.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    Logger::instance().setMinLevel(LogLevel::Warn);

    int width = 800, height = 600;
    auto window = SDL_CreateWindow("Triangle", width, height, 0);
#ifdef CROWY_METALRHI
    auto view = SDL_Metal_CreateView(window);
#endif
    auto device = createDevice();
    initResourceModule(device.get());

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
            .bufferDesc = RHITextureCreateDesc{
                .width = static_cast<uint32_t>(width),
                .height = static_cast<uint32_t>(height),
                .format = RHIPixelFormat::BGRA8_UNORM
            },
            .bufferCount = 3,
            .vsync = true,
            .allowTearing = false
        #if defined(_DEBUG) || !defined(NDEBUG)
            , .debugName = "RHISwapchain"
        #endif
        }
    );
    auto cmdList = device->createCommandList();
    auto framePacer = device->createFramePacer();

    auto [meshHandle, materialSetHandle] = getOrLoad(
        RenderObjectSpec{
            .uri = "file:asset/Stelle/Stelle.pmx"
        }
    );
    auto mesh = get(meshHandle);
    auto materialSet = get(materialSetHandle);

    auto uniformBuffer = device->createBuffer({
        .size = sizeof(Mat4),
        .usage = RHIBufferUsage::ConstantBuffer,
        .access = RHIMemoryAccess::CPUWrite,
        .initialData = nullptr
    }, "MVP Uniform Buffer");

    auto depthBuffer = device->createTexture({
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .depth = 1,
        .mipLevels = 1,
        .arraySize = 1,
        .format = RHIPixelFormat::D32_FLOAT,
        .usage = RHITextureUsage::AllowDepthStencil,
        .initialState = RHIResourceState::DepthWrite,
        .clearColor = {},
        .clearDepthStencil = {1.0f, 0},
    }, "Depth Buffer");

    auto pipelineState = device->createPipelineState({
        .vertexLayout = DEFAULT_VERTEX_ELEMENTS,
    #if defined(CROWY_METALRHI)
        .vertexShaderPath = "asset/Shaders/triangle.metal",
        .vertexShaderEntryPoint = "vs_main",
        .fragmentShaderPath = "asset/Shaders/triangle.metal",
        .fragmentShaderEntryPoint = "fs_textured",
    #elif defined(CROWY_D3DRHI)
        .vertexShaderPath = "asset/Shaders/triangle.hlsl",
        .vertexShaderEntryPoint = "vs_main",
        .fragmentShaderPath = "asset/Shaders/triangle.hlsl",
        .fragmentShaderEntryPoint = "fs_textured",
    #endif
        .depthStencil = RHIDepthStencilState{
            .format = RHIPixelFormat::D32_FLOAT,
            .depthWriteEnable = true
        },
        .renderTargetFormats = {RHIPixelFormat::BGRA8_UNORM},
        .renderTargetCount = 1
    }, "Mesh Pipeline");

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

        if(!framePacer->beginFrame())
            continue;
        if(!swapchain->acquireNextImage()){
            framePacer->endFrame();
            continue;
        }

        auto aspect = float(width)/height;
        auto model = rotate_y_mat(0.5f * et);

        auto camX = std::sin(0.3f * et) * cameraDistance;
        auto camZ = std::cos(0.3f * et) * cameraDistance;
        auto view = look_at(
            Vec3{camX, 10.0f, camZ},
            Vec3{0.0f, 10.0f, 0.0f},
            Vec3{0.0f,  1.0f, 0.0f}
        );

        float fovRad = 45.0f * 3.14159265f / 180.0f;
        auto proj = perspective(fovRad, aspect, 0.1f, cameraDistance * 4.0f);

        auto mvp = proj * view * model;
        uniformBuffer->upload(mvp.data(), sizeof(Mat4));

        cmdList->begin();

        RHIClearColor clearColor{ 0.2f, 0.2f, 0.3f, 1.0f };
        cmdList->beginRenderPass(
            *swapchain,
            depthBuffer.get(),
            RHILoadAction::Clear,
            RHIStoreAction::Store,
            clearColor
        );

        cmdList->setPipelineState(*pipelineState);

        cmdList->setViewport({0, 0, static_cast<float>(width), static_cast<float>(height), 0.0f, 1.0f});
        cmdList->setScissorRect({0, 0, width, height});

        cmdList->setConstantBuffer(*uniformBuffer, 1, RHIShaderStage::VertexShader);

        for(const auto& submesh: mesh){
            cmdList->setVertexBuffer(*submesh.vertexBuffer, 0);
            cmdList->setIndexBuffer(*submesh.indexBuffer);

            auto it = materialSet.find(submesh.materialSlotName);
            if(it == materialSet.end())
                continue;

            cmdList->setTexture(*it->second->baseColorMap, 0,
                RHIBindingAccess::ReadOnly,
                RHIShaderStage::FragmentShader
            );
            cmdList->drawIndexed(submesh.indexCount, 1);
        }
        cmdList->endRenderPass();

        // Signal fence for frame synchronization
        cmdList->signalFence(
            *framePacer->getCurrentFence(),
            framePacer->getNextFenceValue()
        );

        cmdList->close();
        device->submit(*cmdList.get(), swapchain.get());

        framePacer->endFrame();
    }

    framePacer->waitForIdle();

    deinitResourceModule();

    return 0;
}