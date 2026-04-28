#pragma once

#include <d3d11.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif
#include "D3D11Definitions.hpp"
#include "D3D11Util.hpp"

namespace Crowy
{
    class D3D11Swapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:    
        SwapchainRAII swapchain = nullptr;
        bool vsync, allowTearing;
        // Back buffer and RTV for rendering
        TextureRAII backBuffer = nullptr;
        RTVRAII rtv = nullptr;
        // cache for creating rtv
        ID3D11Device& device;

        uint32_t width = 0;
        uint32_t height = 0;
        RHIPixelFormat format = RHIPixelFormat::Unknown;

    public:
        D3D11Swapchain(
            ID3D11Device& device,
            IDXGIFactory2& factory,
            const RHISwapchainCreateDesc& desc
        )
            :device(device)
            ,vsync(desc.vsync), allowTearing(desc.allowTearing)
            , width(desc.bufferDesc.width)
            , height(desc.bufferDesc.height)
            , format(desc.bufferDesc.format)
        {
            DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {
                .Width = desc.bufferDesc.width,
                .Height = desc.bufferDesc.height,
                .Format = convertPixelFormat(desc.bufferDesc.format),
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

            factory.CreateSwapChainForHwnd(
                &device,
                static_cast<HWND>(desc.windowHandle),
                &swapChainDesc,
                nullptr,
                nullptr,
                &swapchain
            );
            createBackBufferResource();
        #if defined(_DEBUG) || !defined(NDEBUG)
            if(!desc.debugName.empty()){
                swapchain->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(desc.debugName.length()),
                    desc.debugName.c_str()
                );
            }
        #endif
        }

        ~D3D11Swapchain() = default;

        bool acquireNextImage() noexcept RHI_OVERRIDE{
            return true;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) noexcept RHI_OVERRIDE{
            if(newWidth == 0 || newHeight == 0)
                return;

            DXGI_SWAP_CHAIN_DESC1 desc;
            swapchain->GetDesc1(&desc);

            swapchain->ResizeBuffers(
                0, newWidth, newHeight,
                DXGI_FORMAT_UNKNOWN, desc.Flags
            );

            createBackBufferResource();
        }

        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }
        uint32_t getWidth() const noexcept RHI_OVERRIDE{
            return width;
        }
        uint32_t getHeight() const noexcept RHI_OVERRIDE{
            return height;
        }

        void* getCurrentNativeTexture() const noexcept RHI_OVERRIDE{
            return getCurrentTexture();
        }

        void present() noexcept{
            UINT syncInterval = vsync ? 1 : 0;
            UINT flags = (!vsync && allowTearing) ?
                DXGI_PRESENT_ALLOW_TEARING : 0;

            swapchain->Present(syncInterval, flags);
        }

        Texture* getCurrentTexture() const noexcept{
            return backBuffer.Get();
        }

        RTV* getCurrentRTV() const noexcept{
            return rtv.Get();
        }

    private:
        void createBackBufferResource(){
            swapchain->GetBuffer(0, __uuidof(ID3D11Texture2D),
                reinterpret_cast<void**>(backBuffer.GetAddressOf())
            );
            device.CreateRenderTargetView(backBuffer.Get(), nullptr, &rtv);
        }
    };
}