#include <imgui_impl_sdl3.h>
#include <SDL3/SDL.h>
#include "math.hpp"
#include "FramePacer.hpp"
#include "Logger.hpp"
#include "RHIDevice.hpp"
#include "RenderDefinitions.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"
#include "Timer.hpp"
#define CROWY_UI_CONTEXT UIContext
#include "UIRenderer.hpp"
#include "Widget.hpp"

namespace Crowy
{
    struct UIContext{
        Renderer& renderer;
    };

    struct CBufferInspector{
        std::string searchStr;

        void submit(UIContext& ctx){
            std::vector<Widget> children;

            children.push_back(SearchBar{
                .label = "CBuffer",
                .onChanged = [this](UIContext&, std::string_view str){
                    searchStr = str;
                },
                .str = searchStr
            });
            if(auto cbuf = ctx.renderer.getCBuffer(searchStr)){
                for(auto [name, field]: cbuf->fieldViews()){
                    switch(field.type){
                    case CBufferFieldType::Int32:
                        children.push_back(IntField{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, int v) mutable{
                                field = v;
                            },
                            .v = field,
                            .get = [field](){ return static_cast<int>(field); }
                        });
                        break;
                    case CBufferFieldType::Float:
                        children.push_back(FloatField{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, float v) mutable{
                                field = v;
                            },
                            .v = field,
                            .get = [field](){ return static_cast<float>(field); }
                        });
                        break;
                    case CBufferFieldType::Float2:
                        children.push_back(Float2Field{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, Vec2 v) mutable{
                                field = v;
                            },
                            .v = field,
                            .get = [field](){ return static_cast<Vec2>(field); }
                        });
                        break;
                    case CBufferFieldType::Float3:
                        children.push_back(Float3Field{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, Vec3 v) mutable{
                                field = v;
                            },
                            .v = field,
                            .get = [field](){ return static_cast<Vec3>(field); }
                        });
                        break;
                    case CBufferFieldType::Float4:
                        children.push_back(Float4Field{
                            .label = std::string(name),
                            .onChanged = [field](UIContext&, Vec4 v) mutable{
                                field = v;
                            },
                            .v = field,
                            .get = [field](){ return static_cast<Vec4>(field); }
                        });
                        break;
                    case CBufferFieldType::Float4x4:
                        break;
                    default:
                        std::unreachable();
                        break;
                    }
                }
            }

            auto w = Column(std::move(children));
            std::visit([&ctx](auto& widget){
                widget.submit(ctx);
            }, w);
        }
    };

    Widget cbufferInspector(std::string_view initCBuf){
        return Box(CBufferInspector{
            .searchStr = std::string(initCBuf)
        });
    }
}

using namespace Crowy;

static CBuffer makeBlackholeParamsCBuffer(
    Vec3 blackholePos, float blackholeMass,
    Vec3 cameraPos, Vec3 cameraTarget,
    float aspect, float tanHalfFov
){
    CBuffer cbuffer{
        .name = "BlackholeParams",
        .slot = 0
    };

    auto camForward = normalize(cameraTarget - cameraPos);
    auto camRight = cross(unit_y(), camForward);
    auto camUp = cross(camForward, camRight);

    // make cbuffer field and initialize
    using enum CBufferFieldType;
    cbuffer.newField("pos", Float3) = blackholePos;
    cbuffer.newField("mass", Float) = blackholeMass;
    cbuffer.newField("camPos", Float3) = cameraPos;
    cbuffer.newField("aspect", Float) = aspect;
    cbuffer.newField("camRight", Float3) = camRight;
    cbuffer.newField("tanHalfFov", Float) = tanHalfFov;
    cbuffer.newField("camUp", Float3) = camUp;
    // implicit 4byte padding
    cbuffer.newField("camForward", Float3) = camForward;

    return cbuffer;
}


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

    Vec3 bhPos{0.0f, 0.0f, 3.0f};
    float bhMass = 1e10;
    Vec3 camPos{10.0, 0.0, 0.0};
    Vec3 camTgt = zeros();
    float fovRad = 45.0f * 3.14159265f / 180.0f;
    float cameraDistance = 30.0f;

    Renderer renderer(device.get());
    UIRenderer uiRenderer(window, *device.get());
    auto ui = Column({
        cbufferInspector("BlackholeParams")
    });

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
                .fs_cbuffers{
                    makeBlackholeParamsCBuffer(
                        bhPos, bhMass,
                        camPos, camTgt,
                        static_cast<float>(width) / height,
                        std::tan(0.5f * fovRad)
                    )
                }
            }
        }
    };
    renderer.loadPasses(spec, width, height);
    auto bhParams = renderer.getCBuffer("BlackholeParams");

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

        // update world data
        camPos = Vec3{
            cameraDistance * std::sin(0.3f * et),
            camPos.y,
            cameraDistance * std::cos(0.3f * et)
        };

        // sync cbuffer
        bhParams->at("camPos") = camPos;
        auto camForward = normalize(camTgt - camPos);
        auto camRight = cross(unit_y(), camForward);
        auto camUp = cross(camForward, camRight);
        bhParams->at("camForward") = camForward;
        bhParams->at("camRight") = camRight;
        bhParams->at("camUp") = camUp;

        auto view = look_at(
            camPos, camTgt,
            Vec3{0.0f,  1.0f, 0.0f}
        );
        auto proj = perspective(
            fovRad,
            float(width)/height,
            0.1f, 100.0f
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

        UIContext uiContext{
            .renderer = renderer
        };
        uiRenderer.render(
            "BlackHole", ui, uiContext,
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

    return 0;
}