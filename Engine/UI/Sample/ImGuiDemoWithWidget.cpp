#include <array>
#include <vector>
#include "AppFramework.hpp"
#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "Primitives.hpp"
#include "PropertyWalker.hpp"
#include "UIRenderer.hpp"

#include <imgui.h>

enum class GalleryShape: Crowy::u8{
    Triangle,
    Square,
    Hexagon
};

namespace Crowy
{
    CROWY_ENUM_BEGIN(GalleryShape)
        CROWY_ENUM_VALUE(Triangle)
        CROWY_ENUM_VALUE(Square)
        CROWY_ENUM_VALUE(Hexagon)
    CROWY_ENUM_END()
}

// Nothing here draws: the point is that the section below it is built from
// this registration alone, with no per-type UI code anywhere.
struct GalleryKnobs{
    GalleryShape shape = GalleryShape::Square;
    Crowy::f32 scale = 1.0f;
    bool wireframe = false;
    Crowy::Str name = "knobs";
};

CROWY_STRUCT(GalleryKnobs)
    .SetProperty("shape", &GalleryKnobs::shape)
    .SetProperty("scale", &GalleryKnobs::scale)
        .SetUIRange(0.1f, 4.0f)
    .SetProperty("wireframe", &GalleryKnobs::wireframe)
    .SetProperty("name", &GalleryKnobs::name)
CROWY_STRUCT_END(GalleryKnobs)

namespace Crowy
{
    struct UIContext{
        bool showDemo = true;
        Color clearColor = {0.09f, 0.11f, 0.14f, 1.0f};
        f64 frameTime = 0.0;
    };

    class ImGuiDemoWithWidget: public App{
    private:
        RAII<UIRenderer> uiRenderer = nullptr;
        UIContext context;
        GalleryKnobs knobs;

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
                },
                // every widget below this line was chosen by property type
                buildPropertyTree(
                    "Reflected", &knobs, *GetDesc<GalleryKnobs>(), []{}
                )
            });
        }

        void OnUpdate(f64 deltaTime, f64) override{
            context.frameTime = deltaTime;
        }

        void OnRecord(RHICommandList& cmdList, const RHIColorAttachment& backBuffer) override{
            uiRenderer->BeginDockSpace();

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
        .title = "ImGuiDemoWithWidget",
        .width = 1280, .height = 720,
        .fullscreen = false,
        .resizable = true,
    };
    return Main<ImGuiDemoWithWidget>(windowConfig);
}
