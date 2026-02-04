#include <imgui_impl_sdl3.h>
#ifdef CROWY_METALRHI
#include <imgui_impl_metal.h>
#elifdef CROWY_D3D11RHI
#include <imgui_impl_dx11.h>
#elifdef CROWY_D3D12RHI
#include <imgui_impl_dx12.h>
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
        #elifdef CROWY_D3D12RHI
            ID3D12Device* device,
            ID3D12CommandQueue* queue
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
        #elifdef CROWY_D3DRHI
            ImGui_ImplSDL3_InitForD3D(window);
        #ifdef CROWY_D3D11RHI
            ImGui_ImplDX11_Init(device, context);
        #elifdef CROWY_D3D12RHI
            ImGui_ImplDX12_InitInfo init_info = {};
            init_info.Device = device;
            init_info.CommandQueue = queue;
            init_info.NumFramesInFlight = RHI_FRAMES_IN_FLIGHT;
            init_info.RTVFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
            init_info.DSVFormat = DXGI_FORMAT_D32_FLOAT;
            init_info.UserData = nullptr;
            init_info.SrvDescriptorHeap = nullptr;
            init_info.SrvDescriptorAllocFn = [](
                ImGui_ImplDX12_InitInfo* info,
                D3D12_CPU_DESCRIPTOR_HANDLE* out_cpu_desc_handle,
                D3D12_GPU_DESCRIPTOR_HANDLE* out_gpu_desc_handle
            ){

            };
            init_info.SrvDescriptorFreeFn = [](
                ImGui_ImplDX12_InitInfo* info,
                D3D12_CPU_DESCRIPTOR_HANDLE cpu_desc_handle, 
                D3D12_GPU_DESCRIPTOR_HANDLE gpu_desc_handle
            ){

            };
            ImGui_ImplDX12_Init(&init_info);
        #endif
        #endif
        }

        ~Impl(){
        #ifdef CROWY_METALRHI
            uiPassDesc->release();

            ImGui_ImplMetal_Shutdown();
        #elifdef CROWY_D3D11RHI
            ImGui_ImplDX11_Shutdown();
        #elifdef CROWY_D3D12RHI
            ImGui_ImplDX12_Shutdown();
        #endif
            ImGui_ImplSDL3_Shutdown();
            ImGui::DestroyContext();
        }

        void render(
            std::string_view uiName,
            Widget& ui,
            CROWY_UI_CONTEXT& uiCtx,
            RHICommandList& cmdList,
            RHISwapchain* backBuffer
        ){
        #ifdef CROWY_METALRHI
            ImGui_ImplMetal_NewFrame(uiPassDesc);
        #elifdef CROWY_D3D11RHI
            ImGui_ImplDX11_NewFrame();
        #elifdef CROWY_D3D12RHI
            ImGui_ImplDX12_NewFrame();
        #endif
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin(uiName.data());
            std::visit([&uiCtx](auto& widget){
                widget.submit(uiCtx);
            }, ui);
            ImGui::End();

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
        #elifdef CROWY_D3D12RHI
            auto commandList = static_cast<ID3D12GraphicsCommandList*>(cmdList.getNative());
            ImGui_ImplDX12_RenderDrawData(draw_data, commandList);
        #endif
        }
    };

    UIRenderer::UIRenderer(void* window, RHIDevice& device)
        :impl(std::make_unique<Impl>(
            static_cast<SDL_Window*>(window),
        #ifdef CROWY_METALRHI
            static_cast<MTL::Device*>(device.getNative())
        #elifdef CROWY_D3D11RHI
            static_cast<ID3D11Device*>(device.getNative()),
            static_cast<ID3D11DeviceContext*>(device.getContextOrQueue())
        #elifdef CROWY_D3D12RHI
            static_cast<ID3D12Device*>(device.getNative()),
            static_cast<ID3D12CommandQueue*>(device.getContextOrQueue())
        #endif
        ))
    {}

    UIRenderer::~UIRenderer() = default;

    void UIRenderer::render(
        std::string_view uiName,
        Widget& ui,
        CROWY_UI_CONTEXT& uiCtx,
        RHICommandList& cmdList,
        RHISwapchain* backBuffer
    ){
        impl->render(uiName, ui, uiCtx, cmdList, backBuffer);
    }
}