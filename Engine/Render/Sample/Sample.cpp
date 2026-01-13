#include <imgui.h>
#include <imgui_impl_metal.h>
#include <imgui_impl_sdl3.h>
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

    int width = 800, height = 600;

    auto window = SDL_CreateWindow("RendererSample", width, height, 0);
    auto device = createDevice();

    initResourceModule(device.get());

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();
    //ImGui::StyleColorsLight();
#ifdef __APPLE__
    ImGui_ImplSDL3_InitForMetal(window);
    ImGui_ImplMetal_Init(static_cast<MTL::Device*>(device->getNative()));

    auto uiPassDesc = MTL::RenderPassDescriptor::alloc()->init();
    auto colorAttachment = uiPassDesc->colorAttachments()->object(0);
    colorAttachment->setLoadAction(MTL::LoadActionLoad);
    colorAttachment->setStoreAction(MTL::StoreActionStore);

    auto view = SDL_Metal_CreateView(window);
#endif
    RHITextureCreateDesc backBufferDesc{
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .format = RHITextureFormat::BGRA8_UNORM
    };

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
            .bufferDesc = backBufferDesc,
            // triple buffering
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

    std::vector<RenderItem> renderItems = {
        RenderItem{
            .mesh = meshHandle,
            .materials = materialSetHandle,
            .world = unitMat(),
            .type = std::hash<RenderType>{}("type0")
        }
    };

    Renderer renderer(device.get());

    RenderSpec spec{
        .renderTargets = {
            {
                "BackBuffer",
                backBufferDesc
            },
            {
                "depth",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
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
        },
        .passes = {
            RenderPassSpec{
                .name = "default",
                // no input texture
                .inputs = {},
                .targets = {
                    "BackBuffer"
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
                .depthStencil = RHIDepthStencilState{},
                .blend = RHIBlendState{}
            },
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
            ImGui_ImplSDL3_ProcessEvent(&event);
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
                auto aspect = float(width)/height;
                renderItems[0].world = rotateYMat(0.5f * et);

                auto camX = std::sin(0.3f * et) * cameraDistance;
                auto camZ = std::cos(0.3f * et) * cameraDistance;
                auto view = lookAt(
                    Vec3{camX, 10.0f, camZ},
                    Vec3{0.0f, 10.0f, 0.0f},
                    Vec3{0.0f,  1.0f, 0.0f}
                );

                float fovRad = 45.0f * 3.14159265f / 180.0f;
                auto proj = perspective(fovRad, aspect, 0.1f, cameraDistance * 4.0f);

                RenderContext ctx{
                    .renderItems = renderItems,
                    .view = view,
                    .proj = proj,
                    .viewport = RHIViewport{
                        .x = 0, .y = 0,
                        .width = static_cast<float>(width),
                        .height = static_cast<float>(height),
                        .minDepth = 0.0f,
                        .maxDepth = 1.0f,
                    }
                };

                cmdList->reset();
                cmdList->begin();

            #ifdef __APPLE__
                // Start the Dear ImGui frame
                ImGui_ImplMetal_NewFrame(uiPassDesc);
                ImGui_ImplSDL3_NewFrame();
            #endif
                ImGui::NewFrame();

                renderer.render(*cmdList.get(), ctx, swapchain.get());

                ImGui::ShowDemoWindow();

                ImGui::Render();
                ImDrawData* draw_data = ImGui::GetDrawData();
            #ifdef __APPLE__
                colorAttachment->setTexture(static_cast<MTL::Texture*>(
                    swapchain->getCurrentNativeTexture()
                ));

                auto commandBuffer = static_cast<MTL::CommandBuffer*>(cmdList->getNativeCommandBuffer());
                auto uiRenderEncoder = commandBuffer->renderCommandEncoder(uiPassDesc);
                ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, uiRenderEncoder);

                uiRenderEncoder->endEncoding();
            #endif

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

#ifdef __APPLE__
    uiPassDesc->release();

    ImGui_ImplMetal_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
#endif

    deinitResourceModule();

    return 0;
}