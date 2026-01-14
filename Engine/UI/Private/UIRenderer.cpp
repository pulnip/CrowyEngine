#include <imgui_impl_sdl3.h>
#ifdef CROWY_METALRHI
#include <imgui_impl_metal.h>
#endif
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"
#include "RHISwapchain.hpp"
#include "UIRenderer.hpp"

namespace Crowy
{
    class UIRenderer::Impl{
    private:
    #ifdef CROWY_METALRHI
        MTL::RenderPassDescriptor* uiPassDesc;
    #endif

    public:
        Impl(SDL_Window* window,
        #ifdef CROWY_METALRHI
            MTL::Device* device
        #endif
        ){
            // Setup Dear ImGui context
            IMGUI_CHECKVERSION();
            ImGui::CreateContext();
            ImGuiIO& io = ImGui::GetIO(); (void)io;
            // Enable Keyboard Controls
            io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;

            // Setup Dear ImGui style
            ImGui::StyleColorsDark();
            //ImGui::StyleColorsLight();

        #ifdef CROWY_METALRHI
            ImGui_ImplSDL3_InitForMetal(window);
            ImGui_ImplMetal_Init(device);

            uiPassDesc = MTL::RenderPassDescriptor::alloc()->init();
            auto colorAttachment = uiPassDesc->colorAttachments()->object(0);
            colorAttachment->setLoadAction(MTL::LoadActionLoad);
            colorAttachment->setStoreAction(MTL::StoreActionStore);
        #endif
        }

        ~Impl(){
        #ifdef CROWY_METALRHI
            uiPassDesc->release();

            ImGui_ImplMetal_Shutdown();
        #endif
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        }

        void render(
            RHICommandList& cmdList,
            std::function<void(void)> uiFunc,
            RHISwapchain* backBuffer
        ){
        #ifdef CROWY_METALRHI
            ImGui_ImplMetal_NewFrame(uiPassDesc);
            ImGui_ImplSDL3_NewFrame();
        #endif
            ImGui::NewFrame();

            uiFunc();

            ImGui::Render();
            ImDrawData* draw_data = ImGui::GetDrawData();
        #ifdef CROWY_METALRHI
            auto colorAttachment = uiPassDesc->colorAttachments()->object(0);
            colorAttachment->setTexture(static_cast<MTL::Texture*>(
                backBuffer->getCurrentNativeTexture()
            ));

            auto commandBuffer = static_cast<MTL::CommandBuffer*>(
                cmdList.getNativeCommandBuffer()
            );
            auto uiRenderEncoder = commandBuffer->renderCommandEncoder(uiPassDesc);
            ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, uiRenderEncoder);

            uiRenderEncoder->endEncoding();
        #endif
        }
    };

    UIRenderer::UIRenderer(void* window, RHIDevice* device)
        :impl(std::make_unique<Impl>(
            static_cast<SDL_Window*>(window),
        #ifdef CROWY_METALRHI
            static_cast<MTL::Device*>(device->getNative())
        #endif
        ))
    {}

    UIRenderer::~UIRenderer() = default;

    void UIRenderer::render(
        RHICommandList& cmdList,
        std::function<void(void)> ctx,
        RHISwapchain* backBuffer
    ){
        impl->render(cmdList, ctx, backBuffer);
    }
}