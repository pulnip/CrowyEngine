#include <stdexcept>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "D3D11Buffer.hpp"
#include "D3D11CommandList.hpp"
#include "D3D11Device.hpp"
#include "D3D11Fence.hpp"
#include "D3D11PipelineState.hpp"
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
        ID3D11Device* device = nullptr;
        IDXGIFactory2* factory = nullptr;

        Impl(){
            using Microsoft::WRL::ComPtr;

            UINT dxgiFactoryFlags = 0;
        #ifdef _DEBUG
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
        #ifdef _DEBUG
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
        }

        ~Impl(){
            if(device != nullptr)
                device->Release();
            if(factory != nullptr)
                factory->Release();
        }

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Buffer>(device, desc);
        }

        RHITexturePtr createTexture(
            const RHITextureCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Texture>(device, desc);
        }

        RHIShaderPtr createShader(
            const RHIShaderCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D11Shader>(device, desc);
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
            return std::make_unique<D3D11CommandList>(device);
        }

        RHIFencePtr createFence(uint64_t initialValue) noexcept{
            return std::make_unique<D3D11Fence>(device, initialValue);
        }

        void submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
        }

        ID3D11Device* getNative() noexcept{ return device; }
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
        return impl->getNative();
    }
}