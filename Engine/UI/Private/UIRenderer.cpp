#include <imgui_impl_sdl3.h>
#ifdef CROWY_METALRHI
#include <imgui_impl_metal.h>
#elifdef CROWY_D3D11RHI
#include <imgui_impl_dx11.h>
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
        #elifdef CROWY_D3D11RHI
            ID3D11Device* device,
            ID3D11DeviceContext* context
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

            ImGui_ImplMetal_Init(static_cast<MTL::Device*>(device));

            uiPassDesc = MTL::RenderPassDescriptor::alloc()->init();
            auto colorAttachment = uiPassDesc->colorAttachments()->object(0);
            colorAttachment->setLoadAction(MTL::LoadActionLoad);
            colorAttachment->setStoreAction(MTL::StoreActionStore);
        #elifdef CROWY_D3D11RHI
            ImGui_ImplSDL3_InitForD3D(window);
            ImGui_ImplDX11_Init(device, context);
        #endif
        }

        ~Impl(){
        #ifdef CROWY_METALRHI
            uiPassDesc->release();

            ImGui_ImplMetal_Shutdown();
        #elifdef CROWY_D3D11RHI
            ImGui_ImplDX11_Shutdown();
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
        #elifdef CROWY_D3D11RHI
            ImGui_ImplDX11_NewFrame();
        #endif
            ImGui_ImplSDL3_NewFrame();
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
                cmdList.getNative()
            );
            auto uiRenderEncoder = commandBuffer->renderCommandEncoder(uiPassDesc);
            ImGui_ImplMetal_RenderDrawData(draw_data, commandBuffer, uiRenderEncoder);

            uiRenderEncoder->endEncoding();
        #elifdef CROWY_D3D11RHI
            cmdList.beginRenderPass(*backBuffer);
            ImGui_ImplDX11_RenderDrawData(draw_data);
            cmdList.endRenderPass();
        #endif
        }
    };

    UIRenderer::UIRenderer(void* window, RHIDevice& device, RHICommandList& cmdList)
        :impl(std::make_unique<Impl>(
            static_cast<SDL_Window*>(window),
        #ifdef CROWY_METALRHI
            static_cast<MTL::Device*>(device.getNative())
        #elifdef CROWY_D3D11RHI
            static_cast<ID3D11Device*>(device.getNative()),
            static_cast<ID3D11DeviceContext*>(cmdList.getNative())
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