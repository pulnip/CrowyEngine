#pragma once

#include <d3d11.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif
#include "D3D11Util.hpp"

namespace Crowy
{
    class D3D11Swapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:    
        IDXGISwapChain1* swapchain = nullptr;
        bool vsync;

    public:
        D3D11Swapchain(
            ID3D11Device* device,
            IDXGIFactory2* factory,
            const RHISwapchainCreateDesc& desc
        )
            :vsync(desc.vsync)
        {
            DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
                .Width = desc.bufferDesc.width,
                .Height = desc.bufferDesc.height,
                .Format = convertTextureFormat(desc.bufferDesc.format),
                .Stereo = FALSE,
                // No MSAA for swapchain
                .SampleDesc = {1, 0},
                .BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
                .BufferCount = desc.bufferCount,
                .Scaling = DXGI_SCALING_STRETCH,
                .SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD,
                .AlphaMode = DXGI_ALPHA_MODE_IGNORE,
                .Flags = desc.allowTearing ?
                    DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : UINT(0)
            };

            factory->CreateSwapChainForHwnd(
                device,
                static_cast<HWND>(desc.windowHandle),
                &swapChainDesc,
                nullptr,
                nullptr,
                &swapchain
            );
        }

        ~D3D11Swapchain(){
            if(swapchain)
                swapchain->Release();
        }

        bool acquireNextImage() noexcept RHI_OVERRIDE{
            return true;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) noexcept RHI_OVERRIDE{

        }

        void* getCurrentNativeTexture() const noexcept RHI_OVERRIDE{
            return nullptr;
        }
    };
}