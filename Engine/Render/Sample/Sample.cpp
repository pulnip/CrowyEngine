#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include "enum_traits.hpp"
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
#include "UIRenderer.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    // Logger::instance().setMinLevel(LogLevel::Warn);

    int width = 800, height = 600;

    auto window = SDL_CreateWindow("RendererSample", width, height, 0);
    auto device = createDevice();

    initResourceModule(device.get());

#ifdef CROWY_METALRHI
    auto view = SDL_Metal_CreateView(window);
#endif
    RHITextureCreateDesc backBufferDesc{
        .width = static_cast<uint32_t>(width),
        .height = static_cast<uint32_t>(height),
        .format = RHITextureFormat::BGRA8_UNORM
    };

    auto swapchain = device->createSwapchain(
        RHISwapchainCreateDesc{
        #ifdef CROWY_METALRHI
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
    UIRenderer uiRenderer(window, device.get());

    RenderSpec spec{
        .renderTargets = {
            {"BackBuffer", backBufferDesc},
            {
                "albedo",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .debugName = "Albedo Texture"
                },
            },
            {
                "normal",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .debugName = "Normal Texture"
                },
            },
            {
                "toonColor",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "Toon Color"
                },
            },
            {
                "sceneColor",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "Scene Color"
                },
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
                    .usage = combine(
                        RHITextureUsage::DepthStencil,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::DepthStencilWrite,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "Depth Buffer"
                },
            },
            {
                "outlined",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "outlined"
                },
            },
            {
                "pixelated",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "pixelated"
                },
            },
            {
                "focusMask",
                RHITextureCreateDesc{
                    .width = static_cast<uint32_t>(width),
                    .height = static_cast<uint32_t>(height),
                    .depth = 1,
                    .mipLevels = 1,
                    .arraySize = 1,
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::ShaderResource,
                    .clearColor = {},
                    .clearDepthStencil = {1.0f, 0},
                    .debugName = "focusMask"
                },
            },
        },
        .passes = {
            RenderPassSpec{
                .name = "gbuffer",
                .inputs = {},
                .targets = {"albedo", "normal"},
                .depthTarget = "depth",
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/gbuffer.metal",
                    .vsFuncName = "vs_gbuffer",
                    .fsFilePath = "asset/Shaders/gbuffer.metal",
                    .fsFuncName = "fs_gbuffer"
                #endif
                },
                .renderType = "type0",
                .rasterizer = RHIRasterizerState{},
                .depthStencil = RHIDepthStencilState{
                    .depthEnable = true,
                    .depthWriteEnable = true
                },
                .blend = RHIBlendState{}
            },
            RenderPassSpec{
                .name = "celshading",
                .inputs = {"albedo", "normal"},
                .targets = {"toonColor"},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/cel_shading.metal",
                    .fsFuncName = "fs_cel_shading"
                #endif
                },
                .rasterizer = RHIRasterizerState{},
                .depthStencil = RHIDepthStencilState{},
                .blend = RHIBlendState{}
            },
            RenderPassSpec{
                .name = "outlining",
                .inputs = {"normal", "depth", "toonColor"},
                .targets = {"outlined"},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/outline.metal",
                    .fsFuncName = "fs_outline"
                #endif
                },
                .rasterizer = RHIRasterizerState{},
                .depthStencil = RHIDepthStencilState{},
                .blend = RHIBlendState{}
            },
            RenderPassSpec{
                .name = "scene",
                // no input texture
                .inputs = {},
                .targets = {"sceneColor"},
                // .depthTarget = "depth",
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/triangle.metal",
                    .vsFuncName = "vs_main",
                    .fsFilePath = "asset/Shaders/triangle.metal",
                    .fsFuncName = "fs_textured",
                #elif CROWY_D3D12RHI
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
            RenderPassSpec{
                .name = "pixelate",
                .inputs = {"sceneColor"},
                .targets = {"pixelated"},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/pixelate.metal",
                    .fsFuncName = "fs_pixelate"
                #endif
                },
            },
            RenderPassSpec{
                .name = "focusmask",
                .inputs = {},
                .targets = {"focusMask"},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/focusmask.metal",
                    .fsFuncName = "fs_focusmask"
                #endif
                }
            },
            RenderPassSpec{
                .name = "composite",
                .inputs = {
                    "outlined",
                    "sceneColor",
                    "focusMask"
                },
                .targets = {"BackBuffer"},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/composite.metal",
                    .fsFuncName = "fs_composite"
                #endif
                }
            }
        }
    };
    renderer.loadPasses(spec);

    float cameraDistance = 30.0f;
    float mouseX, mouseY;
    SDL_GetMouseState(&mouseX, &mouseY);

    Timer timer;
    timer.reset();

    bool isRunning = true;

    // TODO. make this to UI!
    bool enable_pixelate = false;
    bool enable_focusmask = false;
    renderer.setPassEnabled("pixelate", enable_pixelate);
    renderer.setPassEnabled("composite", enable_focusmask);

    while(isRunning){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            ImGui_ImplSDL3_ProcessEvent(&event);
            switch(event.type){
            case SDL_EVENT_QUIT:
                isRunning = false;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                SDL_GetMouseState(&mouseX, &mouseY);
                break;
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

                cmdList->begin();

                renderer.render(*cmdList.get(), ctx, swapchain.get());
                cmdList->flush();

                uiRenderer.render(
                    *cmdList.get(),
                    [&renderer, &enable_pixelate, &enable_focusmask](){
                        ImGui::Begin("Renderer Sample");
                        if(ImGui::Checkbox("Pixelate", &enable_pixelate))
                            renderer.setPassEnabled("pixelate", enable_pixelate);
                        if(ImGui::Checkbox("Focusmask", &enable_focusmask))
                            renderer.setPassEnabled("composite", enable_focusmask);
                        ImGui::End();
                    },
                    swapchain.get()
                );

                // Signal fence for frame synchronization
                cmdList->signalFence(
                    *framePacer->getCurrentFence(),
                    framePacer->getNextFenceValue()
                );

                cmdList->close();
            }
            device->submit(*cmdList.get(), *swapchain.get());

            framePacer->endFrame();
        }
    }

    framePacer->waitForIdle();

    deinitResourceModule();

    return 0;
}