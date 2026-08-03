#include <array>
#include <vector>
#include "AppFramework.hpp"
#include "Primitives.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

namespace Crowy
{
    struct UIContext{
        bool showDemo = true;
        Color clearColor = {0.09f, 0.11f, 0.14f, 1.0f};
        f64 frameTime = 0.0;
    };

    class UISample: public App{
    private:
        RAII<UIRenderer> uiRenderer = nullptr;
        UIContext context;

        Widget panel;

        void OnInit(RHIDevice& device, RHISwapchain& swapchain) override{
            uiRenderer = std::make_unique<UIRenderer>(
                device,
                swapchain.GetFormat()
            );

            panel = Column({
                Text{
                    .data = "RHI 위에서 그리는 ImGui"
                },
                Checkbox{
                    .label = "ImGui Demo",
                    .onChanged = [](UIContext& ctx, bool v){
                        ctx.showDemo = v;
                    },
                    .v = context.showDemo
                },
                Float4Field{
                    .label = "Clear Color",
                    .onChanged = [](UIContext& ctx, Vec4 v){
                        ctx.clearColor = v;
                    },
                    .v = context.clearColor
                },
                Slider{
                    .label = "Alpha",
                    .onChanged = [](UIContext&, f32 v){
                        ImGui::GetStyle().Alpha = v;
                    },
                    .v = 1.0f,
                    .v_min = 0.2f,
                    .v_max = 1.0f
                },
                SearchBar{
                    .label = "한글 입력",
                    .onChanged = [](UIContext&, StrView){}
                },
                FloatField{
                    .label = "ms / frame",
                    .get = [&ctx = context]{
                        return static_cast<f32>(1000.0 * ctx.frameTime);
                    }
                }
            });
        }

        void OnUpdate(f64 deltaTime, f64) override{
            context.frameTime = deltaTime;
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            if(context.showDemo)
                ImGui::ShowDemoWindow(&context.showDemo);

            uiRenderer->Prepare(cmdList, panel, context);

            std::array colorAttachments = {
                RHIColorAttachment{
                    .texture = backBuffer.texture,
                    .loadAction = RHILoadAction::Clear,
                    .storeAction = RHIStoreAction::Store,
                    .clearColor = context.clearColor
                }
            };
            // this pass samples the textures Prepare just updated, so their
            // acquire halves ride along with the backbuffer's
            std::vector<RHITextureBarrier> acquires{
                AcquireBackBuffer(backBuffer)
            };
            acquires.append_range(uiRenderer->TextureAcquires());
            cmdList.BeginRenderPass(RHIRenderPassDesc{
                .colorAttachments = colorAttachments
            }, acquires);

            uiRenderer->Record(cmdList);

            const std::array releases{ReleaseBackBuffer(backBuffer)};
            cmdList.EndRenderPass(releases);
        }
    };
}

int main(void){
    using namespace Crowy;

    const WindowConfig windowConfig{
        .title = "UISample",
        .width = 1280, .height = 720,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<UISample>(windowConfig);
}
