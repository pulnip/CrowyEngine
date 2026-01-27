#include <stdexcept>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "D3D12Buffer.hpp"
#include "D3D12CommandList.hpp"
#include "D3D12Device.hpp"
#include "D3D12Fence.hpp"
#include "D3D12PipelineState.hpp"
#include "D3D12Sampler.hpp"
#include "D3D12Shader.hpp"
#include "D3D12Swapchain.hpp"
#include "D3D12Texture.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<D3D12Device> createDevice() noexcept{
        return std::make_unique<D3D12Device>();
    }
#else
    RHIDevicePtr createDevice() noexcept{
        return std::make_unique<D3D12Device>();
    }
#endif

    struct D3D12Device::Impl{
        ID3D12Device* device = nullptr;
        IDXGIFactory4* factory = nullptr;
        ID3D12CommandQueue* commandQueue = nullptr;

        Impl(){
            UINT dxgiFactoryFlags = 0;
        #ifdef _DEBUG
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

            Microsoft::WRL::ComPtr<ID3D12Debug> debugController;
            if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
                debugController->EnableDebugLayer();
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

                // Check if adapter supports D3D12
                if(SUCCEEDED(D3D12CreateDevice(
                    adapter.Get(),
                    D3D_FEATURE_LEVEL_11_0,
                    __uuidof(ID3D12Device),
                    nullptr
                ))){
                    if(desc.DedicatedVideoMemory > maxDedicatedMemory){
                        maxDedicatedMemory = desc.DedicatedVideoMemory;
                        selectedAdapter = adapter;
                    }
                }
            }

            if(selectedAdapter == nullptr){
                throw std::runtime_error("No compatible D3D12 adapter found");
            }

            if(FAILED(D3D12CreateDevice(
                selectedAdapter.Get(),
                D3D_FEATURE_LEVEL_11_0,
                IID_PPV_ARGS(&device)
            ))){
                throw std::runtime_error("Failed to create D3D12 device");
            }

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            if(FAILED(device->CreateCommandQueue(
                &queueDesc,
                IID_PPV_ARGS(&commandQueue)
            ))){
                throw std::runtime_error("Failed to create command queue");
            }
        }

        ~Impl(){
            if(commandQueue != nullptr){
                commandQueue->Release();
                commandQueue = nullptr;
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
            return std::make_unique<D3D12Buffer>(device, desc);
        }

        RHITexturePtr createTexture(
            const RHITextureCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D12Texture>(device, commandQueue, desc);
        }

        RHIShaderPtr createShader(
            const RHIShaderCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D12Shader>(device, desc);
        }

        RHISamplerPtr createSampler(
            const RHISamplerState& desc
        ) noexcept{
            return std::make_unique<D3D12Sampler>(device, desc);
        }

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ) noexcept{
            return std::make_unique<D3D12PipelineState>(device, desc);
        }

        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc& desc
        ) noexcept{
            return std::make_unique<D3D12PipelineState>(device, desc);
        }

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc& desc
        ) noexcept{
            return std::make_unique<D3D12Swapchain>(commandQueue, desc);
        }

        RHICommandListPtr createCommandList() noexcept{
            return std::make_unique<D3D12CommandList>(device, commandQueue);
        }

        RHIFencePtr createFence(uint64_t initialValue) noexcept{
            return std::make_unique<D3D12Fence>(device, initialValue);
        }

        void submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
            auto& dxCmdList = static_cast<D3D12CommandList&>(cmdList);

            ID3D12CommandList* cmdLists[] = {dxCmdList.get()};
            commandQueue->ExecuteCommandLists(1, cmdLists);

            // TODO
            // static_cast<D3D12Swapchain&>(swapchain).present();
        }

        ID3D12Device* getNative() noexcept{ return device; }
    };

    D3D12Device::D3D12Device()
        :impl(std::make_unique<Impl>()){}

    D3D12Device::~D3D12Device(){}

    RHIBufferPtr D3D12Device::createBuffer(
        const RHIBufferCreateDesc& desc
    ) noexcept{
        return impl->createBuffer(desc);
    }

    RHITexturePtr D3D12Device::createTexture(
        const RHITextureCreateDesc& desc
    ) noexcept{
        return impl->createTexture(desc);
    }

    RHIShaderPtr D3D12Device::createShader(
        const RHIShaderCreateDesc& desc
    ) noexcept{
        return impl->createShader(desc);
    }

    RHIPipelineStatePtr D3D12Device::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ) noexcept{
        return impl->createGraphicsPipelineState(desc);
    }

    RHIPipelineStatePtr D3D12Device::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ) noexcept{
        return impl->createComputePipelineState(desc);
    }

    RHISwapchainPtr D3D12Device::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ) noexcept{
        return impl->createSwapchain(desc);
    }

    RHICommandListPtr D3D12Device::createCommandList() noexcept{
        return impl->createCommandList();
    }

    RHIFencePtr D3D12Device::createFence(uint64_t initialValue) noexcept{
        return impl->createFence(initialValue);
    }

    RHICapabilities D3D12Device::getCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void D3D12Device::submit(RHICommandList& cmdList, RHISwapchain& swapchain) noexcept{
        impl->submit(cmdList, swapchain);
    }

    void* D3D12Device::getNative() noexcept{
        return impl->getNative();
    }
}