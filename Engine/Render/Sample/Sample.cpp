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
#define CROWY_UI_CONTEXT UIContext
#include "UIRenderer.hpp"

namespace Crowy
{
    struct UIContext{
        Renderer& renderer;
    };
}

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
            , .debugName = "RHISwapchain"
        #endif
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
    UIRenderer uiRenderer(window, *device.get());
    auto ui = Column({
        Checkbox{
            .label = "Pixelate",
            .onChanged = [](UIContext& ctx, bool v){
                ctx.renderer.setPassEnabled("pixelate", v);
            }
        },
        Checkbox{
            .label = "Focusmask",
            .onChanged = [](UIContext& ctx, bool v){
                ctx.renderer.setPassEnabled("composite", v);
            }
        }
    });

    using enum CBufferFieldType;

    CBufferSpec pixelateParams{
        .name = "PixelateParams",
        .slot = 0
    };
    pixelateParams.newField("resolution", Float2) = Vec2(800, 600);
    pixelateParams.newField("pixelSize", Float2) = Vec2(4, 4);

    CBufferSpec focusParams{
        .name = "FocusParams",
        .slot = 0
    };
    focusParams.newField("focusCenter", Float2) = Vec2{0.5f, 0.1f};
    focusParams.newField("focusRadius", Float) = 0.05f;
    focusParams.newField("falloff", Float) = 0.1f;
    focusParams.newField("aspectRatio", Float) = 1.33f;

    RenderSpec spec{
        .renderTargets = {
            {"BackBuffer", backBufferDesc},
            {
                "albedo",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "Albedo Texture"
                #endif
                },
            },
            {
                "normal",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "Normal Texture"
                #endif
                },
            },
            {
                "toonColor",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "Toon Color"
                #endif
                },
            },
            {
                "sceneColor",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "Scene Color"
                #endif
                },
            },
            {
                "depth",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::D32_FLOAT,
                    .usage = combine(
                        RHITextureUsage::DepthStencil,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::DepthWrite,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "Depth Buffer"
                #endif
                },
            },
            {
                "outlined",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "outlined"
                #endif
                },
            },
            {
                "pixelated",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "pixelated"
                #endif
                },
            },
            {
                "focusMask",
                RHITextureCreateDesc{
                    .format = RHITextureFormat::BGRA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::RenderTarget,
                        RHITextureUsage::ShaderResource
                    ),
                    .initialState = RHIResourceState::AllShaderResource,
                #if defined(_DEBUG) || !defined(NDEBUG)
                    .debugName = "focusMask"
                #endif
                },
            },
        },
        .passes = {
            RenderPassSpec{
                .name = "gbuffer",
                .inputs = {},
                .targets = {"albedo", "normal"},
                .depthTarget = "depth",
                .fs_samplers = {LINEAR_WRAP_SAMPLER},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/gbuffer.metal",
                    .vsFuncName = "vs_gbuffer",
                    .fsFilePath = "asset/Shaders/gbuffer.metal",
                    .fsFuncName = "fs_gbuffer"
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/gbuffer.hlsl",
                    .vsFuncName = "vs_gbuffer",
                    .fsFilePath = L"asset/Shaders/gbuffer.hlsl",
                    .fsFuncName = "fs_gbuffer"
                #endif
                },
                .renderType = "type0",
                .depthStencil = RHIDepthStencilState{
                    .format = RHITextureFormat::D32_FLOAT,
                    .depthWriteEnable = true
                }
            },
            RenderPassSpec{
                .name = "celshading",
                .inputs = {"albedo", "normal"},
                .targets = {"toonColor"},
                .fs_samplers = {LINEAR_WRAP_SAMPLER},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/cel_shading.metal",
                    .fsFuncName = "fs_cel_shading"
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = L"asset/Shaders/cel_shading.hlsl",
                    .fsFuncName = "fs_cel_shading"
                #endif
                }
            },
            RenderPassSpec{
                .name = "outlining",
                .inputs = {"normal", "depth", "toonColor"},
                .targets = {"outlined"},
                .fs_samplers = {LINEAR_WRAP_SAMPLER},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/outline.metal",
                    .fsFuncName = "fs_outline"
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = L"asset/Shaders/outline.hlsl",
                    .fsFuncName = "fs_outline"
                #endif
                }
            },
            RenderPassSpec{
                .name = "scene",
                // no input texture
                .inputs = {},
                .targets = {"sceneColor"},
                .depthTarget = "depth",
                .fs_samplers = {LINEAR_WRAP_SAMPLER},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/triangle.metal",
                    .vsFuncName = "vs_main",
                    .fsFilePath = "asset/Shaders/triangle.metal",
                    .fsFuncName = "fs_textured",
                #elif CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/standard_vs.hlsl",
                    .vsFuncName = "vs_main",
                    .fsFilePath = L"asset/Shaders/standard_ps.hlsl",
                    .fsFuncName = "ps_textured",
                #endif
                },
                .renderType = "type0",
                .depthStencil = RHIDepthStencilState{
                    .format = RHITextureFormat::D32_FLOAT,
                    .depthWriteEnable = true
                }
            },
            RenderPassSpec{
                .name = "pixelate",
                .inputs = {"sceneColor"},
                .targets = {"pixelated"},
                .fs_samplers = {NEAREST_WRAP_SAMPLER},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/pixelate.metal",
                    .fsFuncName = "fs_pixelate"
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = L"asset/Shaders/pixelate.hlsl",
                    .fsFuncName = "fs_pixelate"
                #endif
                },
                .fs_cbuffers = {pixelateParams}
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
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = L"asset/Shaders/focusmask.hlsl",
                    .fsFuncName = "fs_focusmask"
                #endif
                },
                .fs_cbuffers = {focusParams}
            },
            RenderPassSpec{
                .name = "composite",
                .inputs = {
                    "pixelated",
                    "outlined",
                    "focusMask"
                },
                .targets = {"BackBuffer"},
                .fs_samplers = {LINEAR_WRAP_SAMPLER},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/composite.metal",
                    .fsFuncName = "fs_composite"
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = L"asset/Shaders/composite.hlsl",
                    .fsFuncName = "fs_composite"
                #endif
                }
            }
        }
    };
    renderer.loadPasses(spec, width, height);

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

        if(!framePacer->beginFrame())
            continue;
        if(!swapchain->acquireNextImage()){
            framePacer->endFrame();
            continue;
        }

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

        UIContext uiContext{
            .renderer = renderer
        };

        uiRenderer.render(
            "Renderer Sample", ui, uiContext,
            *cmdList.get(),
            swapchain.get()
        );

        // Signal fence for frame synchronization
        cmdList->signalFence(
            *framePacer->getCurrentFence(),
            framePacer->getNextFenceValue()
        );

        cmdList->close();
        device->submit(*cmdList.get(), *swapchain.get());

        framePacer->endFrame();
    }

    framePacer->waitForIdle();

    deinitResourceModule();

    return 0;
}