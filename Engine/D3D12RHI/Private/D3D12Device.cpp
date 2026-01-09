#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "D3D12Buffer.hpp"
#include "D3D12CommandList.hpp"
#include "D3D12Device.hpp"
#include "D3D12Fence.hpp"
#include "D3D12PipelineState.hpp"
#include "D3D12Shader.hpp"
#include "D3D12Swapchain.hpp"
#include "D3D12Texture.hpp"

using Microsoft::WRL::ComPtr;

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<D3D12Device> createDevice(){
        return std::make_unique<D3D12Device>();
    }
#else
    RHIDevicePtr createDevice(){
        return std::make_unique<D3D12Device>();
    }
#endif

    struct D3D12Device::Impl{
        ComPtr<ID3D12Device> device;
        ComPtr<ID3D12CommandQueue> commandQueue;
        ComPtr<IDXGIFactory4> factory;

        Impl(){
            UINT dxgiFactoryFlags = 0;

        #ifdef _DEBUG
            // Enable debug layer
            ComPtr<ID3D12Debug> debugController;
            if(SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))){
                debugController->EnableDebugLayer();
                dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;

                ComPtr<ID3D12Debug1> debugController1;
                if(SUCCEEDED(debugController.As(&debugController1))){
                    debugController1->SetEnableGPUBasedValidation(TRUE);
                }
            }
        #endif

            if(FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))){
                throw std::runtime_error("Failed to create DXGI factory");
            }

            ComPtr<IDXGIAdapter1> adapter;
            ComPtr<IDXGIAdapter1> selectedAdapter;
            SIZE_T maxDedicatedMemory = 0;

            for(UINT i = 0; factory->EnumAdapters1(i, &adapter) != DXGI_ERROR_NOT_FOUND; ++i){
                DXGI_ADAPTER_DESC1 desc;
                adapter->GetDesc1(&desc);

                // Skip software adapter
                if(desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE){
                    continue;
                }

                // Check if adapter supports D3D12
                if(SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, __uuidof(ID3D12Device), nullptr))){
                    if(desc.DedicatedVideoMemory > maxDedicatedMemory){
                        maxDedicatedMemory = desc.DedicatedVideoMemory;
                        selectedAdapter = adapter;
                    }
                }
            }

            if(!selectedAdapter){
                throw std::runtime_error("No compatible D3D12 adapter found");
            }

            if(FAILED(D3D12CreateDevice(selectedAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))){
                throw std::runtime_error("Failed to create D3D12 device");
            }

            // Create command queue
            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;

            if(FAILED(device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue)))){
                throw std::runtime_error("Failed to create command queue");
            }
        }

        ~Impl() = default;

        RHIBufferPtr createBuffer(
            const RHIBufferCreateDesc& desc
        ){
            return std::make_unique<D3D12Buffer>(device.Get(), desc);
        }

        RHITexturePtr createTexture(
            const RHITextureCreateDesc& desc
        ){
            return std::make_unique<D3D12Texture>(device.Get(), commandQueue.Get(), desc);
        }

        RHIShaderPtr createShader(
            const RHIShaderCreateDesc& desc
        ){
            return std::make_unique<D3D12Shader>(device.Get(), desc);
        }

        RHIPipelineStatePtr createGraphicsPipelineState(
            const RHIGraphicsPipelineStateDesc& desc
        ){
            return std::make_unique<D3D12PipelineState>(device.Get(), desc);
        }

        RHIPipelineStatePtr createComputePipelineState(
            const RHIComputePipelineStateDesc& desc
        ){
            return std::make_unique<D3D12PipelineState>(device.Get(), desc);
        }

        RHISwapchainPtr createSwapchain(
            const RHISwapchainCreateDesc& desc
        ){
            return std::make_unique<D3D12Swapchain>(commandQueue.Get(), desc);
        }

        RHICommandListPtr createCommandList(){
            return std::make_unique<D3D12CommandList>(device.Get(), commandQueue.Get());
        }

        RHIFencePtr createFence(uint64_t initialValue){
            return std::make_unique<D3D12Fence>(device.Get(), initialValue);
        }

        void submit(RHICommandList* cmdList, RHISwapchain* swapchain){
            auto d3dCmdList = static_cast<D3D12CommandList*>(cmdList);
            ID3D12CommandList* cmdLists[] = { d3dCmdList->get() };
            commandQueue->ExecuteCommandLists(1, cmdLists);

            // Signal internal fence for allocator synchronization
            d3dCmdList->signalAllocatorFence();

            if(swapchain){
                auto d3dSwapchain = static_cast<D3D12Swapchain*>(swapchain);
                d3dSwapchain->get()->Present(1, 0);
            }

            if(auto fence = d3dCmdList->getPendingFence()){
                auto d3dFence = static_cast<D3D12Fence*>(fence);
                commandQueue->Signal(d3dFence->get(), d3dCmdList->getPendingFenceValue());
            }
        }
    };

    D3D12Device::D3D12Device()
        :impl(std::make_unique<Impl>()){}

    D3D12Device::~D3D12Device(){}

    RHIBufferPtr D3D12Device::createBuffer(
        const RHIBufferCreateDesc& desc
    ){
        return impl->createBuffer(desc);
    }

    RHITexturePtr D3D12Device::createTexture(
        const RHITextureCreateDesc& desc
    ){
        return impl->createTexture(desc);
    }

    RHIShaderPtr D3D12Device::createShader(
        const RHIShaderCreateDesc& desc
    ){
        return impl->createShader(desc);
    }

    RHIPipelineStatePtr D3D12Device::createGraphicsPipelineState(
        const RHIGraphicsPipelineStateDesc& desc
    ){
        return impl->createGraphicsPipelineState(desc);
    }

    RHIPipelineStatePtr D3D12Device::createComputePipelineState(
        const RHIComputePipelineStateDesc& desc
    ){
        return impl->createComputePipelineState(desc);
    }

    RHISwapchainPtr D3D12Device::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ){
        return impl->createSwapchain(desc);
    }

    RHICommandListPtr D3D12Device::createCommandList(){
        return impl->createCommandList();
    }

    RHIFencePtr D3D12Device::createFence(uint64_t initialValue){
        return impl->createFence(initialValue);
    }

    RHICapabilities D3D12Device::getCapabilities() const{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void D3D12Device::submit(RHICommandList* cmdList, RHISwapchain* swapchain){
        impl->submit(cmdList, swapchain);
    }
}