#include <stdexcept>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "D3D11Buffer.hpp"
#include "D3D11CommandList.hpp"
#include "D3D11Device.hpp"
#include "D3D11Fence.hpp"
#include "D3D11PipelineState.hpp"
#include "D3D11Sampler.hpp"
#include "D3D11Shader.hpp"
#include "D3D11Swapchain.hpp"
#include "D3D11Texture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<D3D11Device> createDevice() noexcept{
        return std::make_unique<D3D11Device>();
    }
#else
    RHIDevicePtr createDevice() noexcept{
        return std::make_unique<D3D11Device>();
    }
#endif

    struct D3D11Device::Impl{
        IDXGIFactory2* factory = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* context = nullptr;

        Impl(){
            using Microsoft::WRL::ComPtr;

            UINT dxgiFactoryFlags = 0;
        #if defined(_DEBUG) || !defined(NDEBUG)
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        #endif

            if(FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))){
                throw std::runtime_error("Failed to create DXGI factory");
            }

            ComPtr<IDXGIAdapter1> adapter;
            ComPtr<IDXGIAdapter1> selectedAdapter;
            SIZE_T maxDedicatedMemory = 0;

            for(UINT i=0; factory->EnumAdapters1(i, &adapter)!=DXGI_ERROR_NOT_FOUND; ++i){
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
                    continue;

                // Check if adapter supports D3D11
                if(SUCCEEDED(D3D11CreateDevice(
                    adapter.Get(),
                    D3D_DRIVER_TYPE_UNKNOWN,
                    nullptr,
                    0,
                    nullptr, 0,
                    D3D11_SDK_VERSION,
                    nullptr,
                    nullptr,
                    nullptr
                ))){
                    if(desc.DedicatedVideoMemory > maxDedicatedMemory){
                        maxDedicatedMemory = desc.DedicatedVideoMemory;
                        selectedAdapter = adapter;
                    }
                }
            }

            if(selectedAdapter == nullptr){
                throw std::runtime_error("No compatible D3D11 adapter found");
            }

            UINT deviceFlags = 0;
        #if defined(_DEBUG) || !defined(NDEBUG)
            deviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif
            D3D_FEATURE_LEVEL featureLevels[] = {
                D3D_FEATURE_LEVEL_11_1,
                D3D_FEATURE_LEVEL_11_0,
            }, actualLevel;

            if(FAILED(D3D11CreateDevice(
                selectedAdapter.Get(),
                D3D_DRIVER_TYPE_UNKNOWN,
                nullptr,
                deviceFlags,
                featureLevels,
                _countof(featureLevels),
                D3D11_SDK_VERSION,
                &device,
                &actualLevel,
                // take immediate context later
                nullptr
            ))){
                throw std::runtime_error("Failed to create D3D11 device");
            }

            device->GetImmediateContext(&context);
        }

        ~Impl(){
            if(context != nullptr){
                context->Release();
                context = nullptr;
            }
            if(device != nullptr){
                device->Release();
                device = nullptr;
            }
            if(factory != nullptr){
                factory->Release();
                factory = nullptr;
            }
        }

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Buffer>(device, context, desc);
        }

        RHITexturePtr createTexture(
            const RHITextureCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Texture>(device, context, desc);
        }

        RHIShaderPtr createShader(
            const RHIShaderCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Shader>(device, desc);
        }

        RHISamplerPtr createSampler(
            const RHISamplerState& desc
        ) noexcept{
            return std::make_unique<D3D11Sampler>(device, desc);
        }

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11PipelineState>(device, desc);
        }

        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11PipelineState>(device, desc);
        }

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Swapchain>(device, factory, desc);
        }

        RHICommandListPtr createCommandList() noexcept{
            return std::make_unique<D3D11CommandList>(device, context);
        }

        RHIFencePtr createFence(uint64_t initialValue) noexcept{
            return std::make_unique<D3D11Fence>(device, initialValue);
        }

        void submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
            static_cast<D3D11Swapchain&>(swapchain).present();
        }

        ID3D11Device* get() noexcept{ return device; }
        ID3D11DeviceContext* getContext() noexcept{ return context; }
    };

    D3D11Device::D3D11Device()
        :impl(std::make_unique<Impl>()){}

    D3D11Device::~D3D11Device(){}

    RHIBufferPtr D3D11Device::createBuffer(
        const RHIBufferCreateDesc& desc
    ) noexcept{
        return impl->createBuffer(desc);
    }

    RHITexturePtr D3D11Device::createTexture(
        const RHITextureCreateDesc& desc
    ) noexcept{
        return impl->createTexture(desc);
    }

    RHIShaderPtr D3D11Device::createShader(
        const RHIShaderCreateDesc& desc
    ) noexcept{
        return impl->createShader(desc);
    }

    RHISamplerPtr D3D11Device::createSampler(
        const RHISamplerState& desc
    ) noexcept{
        return impl->createSampler(desc);
    }

    RHIPipelineStatePtr D3D11Device::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ) noexcept{
        return impl->createGraphicsPipelineState(desc);
    }

    RHIPipelineStatePtr D3D11Device::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ) noexcept{
        return impl->createComputePipelineState(desc);
    }

    RHISwapchainPtr D3D11Device::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return impl->createSwapchain(desc);
    }

    RHICommandListPtr D3D11Device::createCommandList() noexcept{
        return impl->createCommandList();
    }

    RHIFencePtr D3D11Device::createFence(uint64_t initialValue) noexcept{
        return impl->createFence(initialValue);
    }

    RHICapabilities D3D11Device::getCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void D3D11Device::submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
        impl->submit(cmdList, swapchain);
    }

    void* D3D11Device::getNative() noexcept{
        return impl->get();
    }
    void* D3D11Device::getContextOrQueue() noexcept{
        return impl->getContext();
    }
}