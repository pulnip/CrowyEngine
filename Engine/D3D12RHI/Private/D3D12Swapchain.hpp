#pragma once

#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <vector>
#include "D3D12Util.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHISwapchain.hpp"
#endif

using Microsoft::WRL::ComPtr;

namespace Crowy
{
    class D3D12Swapchain
#ifndef USE_STATIC_RHI
        : public RHISwapchain
#endif
    {
    private:
        ComPtr<IDXGISwapChain3> swapchain;
        std::vector<ComPtr<ID3D12Resource>> backBuffers;
        uint32_t currentBackBufferIndex = 0;

        uint32_t width = 0;
        uint32_t height = 0;
        RHITextureFormat format = RHITextureFormat::Unknown;
        uint32_t bufferCount = 0;
        bool vsyncEnabled = true;
        bool tearingSupported = false;

    public:
        D3D12Swapchain(
            ID3D12CommandQueue* commandQueue,
            const RHISwapchainCreateDesc& desc
        )
#ifndef USE_STATIC_RHI
            : RHISwapchain(desc.width, desc.height, desc.bufferCount, desc.format)
            ,
#else
            :
#endif
              width(desc.width)
            , height(desc.height)
            , format(desc.format)
            , bufferCount(desc.bufferCount)
            , vsyncEnabled(desc.vsync)
        {
            HWND hwnd = static_cast<HWND>(desc.windowHandle);
            if(!hwnd){
                throw std::runtime_error("Swapchain window handle is null");
            }

            UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif

            ComPtr<IDXGIFactory4> factory;
            if(FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))){
                throw std::runtime_error("Failed to create DXGI factory");
            }

            if(desc.allowTearing){
                ComPtr<IDXGIFactory5> factory5;
                if(SUCCEEDED(factory.As(&factory5))){
                    BOOL allowTearing = FALSE;
                    if(SUCCEEDED(factory5->CheckFeatureSupport(
                        DXGI_FEATURE_PRESENT_ALLOW_TEARING,
                        &allowTearing, sizeof(allowTearing)
                    ))){
                        tearingSupported = (allowTearing == TRUE);
                    }
                }
            }

            DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
            swapchainDesc.Width = desc.width;
            swapchainDesc.Height = desc.height;
            swapchainDesc.Format = convertTextureFormat(desc.format);
            swapchainDesc.Stereo = FALSE;
            swapchainDesc.SampleDesc.Count = 1;
            swapchainDesc.SampleDesc.Quality = 0;
            swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapchainDesc.BufferCount = desc.bufferCount;
            swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
            swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
            swapchainDesc.Flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

            ComPtr<IDXGISwapChain1> swapchain1;
            if(FAILED(factory->CreateSwapChainForHwnd(
                commandQueue,
                hwnd,
                &swapchainDesc,
                nullptr,
                nullptr,
                &swapchain1
            ))){
                throw std::runtime_error("Failed to create swapchain");
            }

            if(FAILED(swapchain1.As(&swapchain))){
                throw std::runtime_error("Failed to query IDXGISwapChain3");
            }

            factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

            createBackBuffers();
        }

        ~D3D12Swapchain() = default;

        bool acquireNextImage() RHI_OVERRIDE{
            currentBackBufferIndex = swapchain->GetCurrentBackBufferIndex();
            return true;
        }

        void resize(uint32_t newWidth, uint32_t newHeight) RHI_OVERRIDE{
            if(newWidth == 0 || newHeight == 0) return;
            
            width = newWidth;
            height = newHeight;

            for(auto& buffer : backBuffers){
                buffer.Reset();
            }
            backBuffers.clear();

            UINT flags = tearingSupported ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
            if(FAILED(swapchain->ResizeBuffers(
                bufferCount,
                newWidth,
                newHeight,
                convertTextureFormat(format),
                flags
            ))){
                throw std::runtime_error("Failed to resize swapchain");
            }

            createBackBuffers();
        }

        IDXGISwapChain3* get() const{
            return swapchain.Get();
        }

        ID3D12Resource* getCurrentBackBuffer() const{
            return backBuffers[currentBackBufferIndex].Get();
        }

        uint32_t getCurrentBackBufferIndex() const{
            return currentBackBufferIndex;
        }

        uint32_t getWidth() const{ return width; }
        uint32_t getHeight() const{ return height; }
        
        // VSync 및 Tearing 관련
        bool isVSyncEnabled() const{ return vsyncEnabled; }
        bool isTearingSupported() const{ return tearingSupported; }
        
        UINT getPresentFlags() const{
            if(!vsyncEnabled && tearingSupported){
                return DXGI_PRESENT_ALLOW_TEARING;
            }
            return 0;
        }
        
        UINT getSyncInterval() const{
            return vsyncEnabled ? 1 : 0;
        }

    private:
        void createBackBuffers(){
            backBuffers.resize(bufferCount);
            for(uint32_t i = 0; i < bufferCount; ++i){
                if(FAILED(swapchain->GetBuffer(i, IID_PPV_ARGS(&backBuffers[i])))){
                    throw std::runtime_error("Failed to get swapchain back buffer");
                }
            }
            currentBackBufferIndex = swapchain->GetCurrentBackBufferIndex();
        }
    };
}