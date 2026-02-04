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
#include "DescriptorHeapAllocator.hpp"

namespace Crowy
{
    class D3D12Swapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:
        IDXGISwapChain3* swapchain = nullptr;
        DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN;
        bool vsync, allowTearing = false;
        // cache for creating rtv
        DescriptorHeapAllocator* allocator = nullptr;
        UINT indexes[RHI_FRAMES_IN_FLIGHT] = {UINT_MAX, UINT_MAX, UINT_MAX};

    public:
        D3D12Swapchain(
            ID3D12CommandQueue* commandQueue,
            IDXGIFactory4* factory,
            const RHISwapchainCreateDesc& desc,
            DescriptorHeapAllocator* allocator
        )
            : format(convert(desc.bufferDesc.format))
            , vsync(desc.vsync), allocator(allocator)
        {
            if(desc.allowTearing){
                IDXGIFactory5* factory5 = nullptr;
                if(SUCCEEDED(factory->QueryInterface(
                    IID_PPV_ARGS(&factory5)
                ))){
                    factory5->CheckFeatureSupport(
                        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                        &allowTearing, sizeof(allowTearing)
                    );
                }
            }

            DXGI_SWAP_CHAIN_DESC1 swapchainDesc{
                .Width = desc.bufferDesc.width,
                .Height = desc.bufferDesc.height,
                .Format = format,
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

            IDXGISwapChain1* swapchain1 = nullptr;
            if(FAILED(factory->CreateSwapChainForHwnd(
                commandQueue,
                static_cast<HWND>(desc.windowHandle),
                &swapchainDesc,
                nullptr,
                nullptr,
                &swapchain1
            ))){
                throw std::runtime_error("Failed to create swapchain");
            }

            if(FAILED(swapchain1->QueryInterface(
                IID_PPV_ARGS(&swapchain)
            ))){
                throw std::runtime_error("IDXGISwapChain3 not support");
            }

            for(UINT i = 0; i < swapchainDesc.BufferCount; ++i){
                ID3D12Resource* backBuffer = nullptr;
                swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffer));

                D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{
                    .Format = format,
                    .ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D,
                    .Texture2D = {
                        .MipSlice = 0,
                        .PlaneSlice = 0
                    }
                };
                indexes[i] = allocator->allocate(backBuffer, rtvDesc);
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

        DXGI_FORMAT getFormat() const{ return format; }
        UINT getRTVHeapIndex() const{ return indexes[swapchain->GetCurrentBackBufferIndex()]; }
    };
}