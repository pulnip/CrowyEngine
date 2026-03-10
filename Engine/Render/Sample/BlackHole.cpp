#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include "FramePacer.hpp"
#include "Logger.hpp"
#include "RHIDevice.hpp"
#include "RenderDefinitions.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"
#include "Timer.hpp"
#include "math.hpp"
// #define CROWY_UI_CONTEXT UIContext
// #include "UIRenderer.hpp"

// namespace Crowy
// {
//     struct UIContext{
//         Renderer& renderer;
//     };
// }

using namespace Crowy;

int main(int argc, char* argv[]){
    Logger::instance().setMinLevel(LogLevel::Warn);

    int width = 800, height = 600;

    auto window = SDL_CreateWindow("BlackHole", width, height, 0);
    auto device = createDevice();

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

    struct Blackhole{
        Vec3 pos{0.5f, 0.2f, 0.3f};
        float mass = 1e10;
    } blackhole;

    struct Camera{
        Vec3 pos = zeros();
        Vec4 rot = unit_quat();
    } camera;
    float fovRad = 45.0f * 3.14159265f / 180.0f;

    Renderer renderer(device.get());
    // UIRenderer uiRenderer(window, *device.get());
    // auto ui = Column({
    //     Checkbox{
    //         .label = "Pixelate",
    //         .onChanged = [](UIContext& ctx, bool v){
    //             ctx.renderer.setPassEnabled("pixelate", v);
    //         }
    //     },
    //     Checkbox{
    //         .label = "Focusmask",
    //         .onChanged = [](UIContext& ctx, bool v){
    //             ctx.renderer.setPassEnabled("composite", v);
    //         }
    //     }
    // });

    using enum CBufferFieldType;

    CBuffer bhParams{
        .name = "BlackholeParams",
        .slot = 0
    };

    bhParams.newField("pos", Float3) = blackhole.pos;
    bhParams.newField("mass", Float) = blackhole.mass;
    bhParams.newField("camPos", Float3) = camera.pos;
    bhParams.newField("aspect", Float) = static_cast<float>(width) / height;
    bhParams.newField("camRight", Float3) = right(camera.rot);
    bhParams.newField("tanHalfFov", Float) = std::tan(0.5f * fovRad);
    bhParams.newField("camUp", Float3) = up(camera.rot);
    // implicit 4byte padding
    bhParams.newField("camForward", Float3) = forward(camera.rot);

    RenderSpec spec{
        .renderTargets = {
            {"BackBuffer", backBufferDesc},
        },
        .passes = {
            RenderPassSpec{
                .name = "main",
                .inputs = {},
                .targets = {"BackBuffer"},
                .shader = ShaderSpec{
                #ifdef CROWY_METALRHI
                    .vsFilePath = "asset/Shaders/fullscreen.metal",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = "asset/Shaders/blackhole.metal",
                    .fsFuncName = "fs_blackhole"
                #elifdef CROWY_D3DRHI
                    .vsFilePath = L"asset/Shaders/fullscreen.hlsl",
                    .vsFuncName = "vs_fullscreen",
                    .fsFilePath = L"asset/Shaders/blackhole.hlsl",
                    .fsFuncName = "fs_blackhole"
                #endif
                },
                .fs_cbuffers{bhParams}
            }
        }
    };
    renderer.loadPasses(spec, width, height);

    float cameraDistance = 30.0f;
    // float mouseX, mouseY;
    // SDL_GetMouseState(&mouseX, &mouseY);

    Timer timer;
    timer.reset();

    bool isRunning = true;

    while(isRunning){
        SDL_Event event;
        while(SDL_PollEvent(&event)){
            // ImGui_ImplSDL3_ProcessEvent(&event);
            switch(event.type){
            case SDL_EVENT_QUIT:
                isRunning = false;
                break;
            case SDL_EVENT_MOUSE_MOTION:
                // SDL_GetMouseState(&mouseX, &mouseY);
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

        // renderItems[0].world = rotate_y_mat(0.5f * et);

        auto view = look_at(
            camera.pos,
            Vec3{0.0f, 0.0f, 1.0f},
            Vec3{0.0f,  1.0f, 0.0f}
        );
        auto proj = perspective(
            fovRad,
            float(width)/height,
            0.1f, cameraDistance * 4.0f
        );

        RenderContext ctx{
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

        // UIContext uiContext{
        //     .renderer = renderer
        // };
        // uiRenderer.render(
        //     "BlackHole", ui, uiContext,
        //     *cmdList.get(),
        //     swapchain.get()
        // );

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

    return 0;
}