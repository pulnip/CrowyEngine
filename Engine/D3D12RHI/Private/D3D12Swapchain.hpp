#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif
#include "D3D12Util.hpp"

namespace Crowy
{
    class D3D12Swapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:
        IDXGISwapChain1* swapchain = nullptr;
        bool vsync, allowTearing = false;
        // cache for creating rtv
        DescriptorHeapAllocator* allocator = nullptr;

    public:
        D3D12Swapchain(
            ID3D12CommandQueue* commandQueue,
            IDXGIFactory4* factory,
            const RHISwapchainCreateDesc& desc,
            DescriptorHeapAllocator* allocator
        )
            :vsync(desc.vsync),allocator(allocator)
        {
            if(desc.allowTearing){
                auto factory5 = dynamic_cast<IDXGIFactory5*>(factory);
                if(factory5 != nullptr){
                    factory5->CheckFeatureSupport(
                        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                        &allowTearing, sizeof(allowTearing)
                    );
                }
            }

            DXGI_SWAP_CHAIN_DESC1 swapchainDesc{
                .Width = desc.bufferDesc.width,
                .Height = desc.bufferDesc.height,
                .Format = convert(desc.bufferDesc.format),
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

            if(FAILED(factory->CreateSwapChainForHwnd(
                commandQueue,
                static_cast<HWND>(desc.windowHandle),
                &swapchainDesc,
                nullptr,
                nullptr,
                &swapchain
            ))){
                throw std::runtime_error("Failed to create swapchain");
            }

            if(desc.debugName){
                swapchain->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(strlen(desc.debugName)),
                    desc.debugName
                );
            }
        }

        ~D3D12Swapchain(){
            if(swapchain){
                swapchain->Release();
                swapchain = nullptr;
            }
        }

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
        }

        void* getCurrentNativeTexture() const noexcept RHI_OVERRIDE{
            // this api for Metal
            return nullptr;
        }

        void present() noexcept{
            UINT syncInterval = vsync ? 1 : 0;
            UINT flags = (!vsync && allowTearing) ?
                DXGI_PRESENT_ALLOW_TEARING : 0;

            swapchain->Present(syncInterval, flags);
        }
    };
}