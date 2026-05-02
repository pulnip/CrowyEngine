#include <stdexcept>
#include <d3d11.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include "D3D11Buffer.hpp"
#include "D3D11CommandList.hpp"
#include "D3D11Definitions.hpp"
#include "D3D11Device.hpp"
#include "D3D11Fence.hpp"
#include "D3D11FrameScope.hpp"
#include "D3D11PipelineState.hpp"
#include "D3D11Sampler.hpp"
#include "D3D11Swapchain.hpp"
#include "D3D11Texture.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"

namespace Crowy
{
#ifdef USE_STATIC_RHI
    std::unique_ptr<D3D11Device> createDevice(){
        return std::make_unique<D3D11Device>();
    }
#else
    RHIDeviceRAII createDevice(){
        return std::make_unique<D3D11Device>();
    }
#endif

    struct D3D11Device::Impl{
        FactoryRAII factory = nullptr;
        DeviceRAII device = nullptr;
        DeviceContextRAII context = nullptr;

        Impl(){
            UINT dxgiFactoryFlags = 0;
        #if defined(_DEBUG) || !defined(NDEBUG)
            dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
        #endif

            if(FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)))){
                throw std::runtime_error("Failed to create DXGI factory");
            }

            AdapterRAII adapter, selectedAdapter;
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
                device.GetAddressOf(),
                &actualLevel,
                // take immediate context later
                nullptr
            ))){
                throw std::runtime_error("Failed to create D3D11 device");
            }

            device->GetImmediateContext(&context);
        }

        ~Impl()= default;

        RHIFrameScopeRAII createFrameScopoe() noexcept{
            return std::make_unique<D3D11FrameScope>();
        }

        RHIBufferRAII createBuffer(
            const RHIBufferCreateDesc& desc,
            const std::string& name
        ){
            return std::make_unique<D3D11Buffer>(
                *device.Get(), *context.Get(),
                desc, name
            );
        }

        RHITextureRAII createTexture(
            const RHITextureCreateDesc& desc,
            const std::string& name
        ){
            return std::make_unique<D3D11Texture>(
                *device.Get(), *context.Get(),
                desc, name
            );
        }

        RHISamplerRAII createSampler(
            const RHISamplerState& desc
        ){
            return std::make_unique<D3D11Sampler>(*device.Get(), desc);
        }

        RHIGraphicsPipelineStateRAII createPipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            const std::string& name
        ){
            return std::make_unique<D3D11GraphicsPipelineState>(*device.Get(), desc, name);
        }

        RHIComputePipelineStateRAII createPipelineState(
            const RHIComputePipelineStateDesc& desc,
            const std::string& name
        ){
            return std::make_unique<D3D11ComputePipelineState>(*device.Get(), desc, name);
        }

        RHISwapchainRAII createSwapchain(
            const RHISwapchainCreateDesc& desc
        ){
            return std::make_unique<D3D11Swapchain>(*device.Get(), *factory.Get(), desc);
        }

        RHICommandListRAII createCommandList(){
            return std::make_unique<D3D11CommandList>(*context.Get());
        }

        RHIFenceRAII createFence(uint64_t initialValue){
            return std::make_unique<D3D11Fence>(*device.Get(), initialValue);
        }

        void submit(RHICommandList& cmdList, RHISwapchain* swapchain) noexcept{
            if(swapchain != nullptr)
                static_cast<D3D11Swapchain&>(*swapchain).present();
        }

        Device* get() noexcept{ return device.Get(); }
        DeviceContext* getContext() noexcept{ return context.Get(); }
    };

    D3D11Device::D3D11Device()
        :impl(std::make_unique<Impl>()){}

    D3D11Device::~D3D11Device(){}

    RHIFrameScopeRAII D3D11Device::createFrameScope(){
        return impl->createFrameScopoe();
    }

    RHIBufferRAII D3D11Device::createBuffer(
        const RHIBufferCreateDesc& desc,
        const std::string& name
    ){
        return impl->createBuffer(desc, name);
    }

    RHITextureRAII D3D11Device::createTexture(
        const RHITextureCreateDesc& desc,
        const std::string& name
    ){
        return impl->createTexture(desc, name);
    }

    RHISamplerRAII D3D11Device::createSampler(
        const RHISamplerState& desc
    ){
        return impl->createSampler(desc);
    }

    RHIGraphicsPipelineStateRAII D3D11Device::createPipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        const std::string& name
    ){
        return impl->createPipelineState(desc, name);
    }

    RHIComputePipelineStateRAII D3D11Device::createPipelineState(
        const RHIComputePipelineStateDesc& desc,
        const std::string& name
    ){
        return impl->createPipelineState(desc, name);
    }

    RHISwapchainRAII D3D11Device::createSwapchain(
        const RHISwapchainCreateDesc& desc
    ){
        return impl->createSwapchain(desc);
    }

    RHICommandListRAII D3D11Device::createCommandList(){
        return impl->createCommandList();
    }

    RHIFenceRAII D3D11Device::createFence(uint64_t initialValue){
        return impl->createFence(initialValue);
    }

    RHICapabilities D3D11Device::getCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void D3D11Device::submit(RHICommandList& cmdList, RHISwapchain* swapchain) noexcept{
        impl->submit(cmdList, swapchain);
    }

    void* D3D11Device::getNative() noexcept{
        return impl->get();
    }
    void* D3D11Device::getContextOrQueue() noexcept{
        return impl->getContext();
    }
}