#include "RHIDefinitions.hpp"
extern "C"{
    // Debug AutoreleasePool
    void _objc_autoreleasePoolPrint(void);
}

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/Metal.hpp>
#include "Assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalDevice.hpp"
#include "MetalFence.hpp"
#include "MetalFrameScope.hpp"
#include "MetalPipelineState.hpp"
#include "MetalSampler.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"

namespace Crowy
{
    RHIDeviceRAII CreateMetalDevice(){
        return std::make_unique<MetalDevice>();
    }

    class MetalDevice::Impl{
    private:
        MTL::Device* device;
        MTL::CommandQueue* commandQueue;
        RAII<MetalCommandList> cmdList;

        AutoreleasePoolScope autoreleasePool;

    public:
        Impl(){
            device = MTL::CreateSystemDefaultDevice();
            SMOL_ASSERT(device != nullptr, "No GPU Available");

            commandQueue = device->newCommandQueue();
            SMOL_ASSERT(commandQueue != nullptr,
                "Failed to create command queue"
            );

            cmdList = std::make_unique<MetalCommandList>(
                commandQueue
            );
        }

        ~Impl(){
            cmdList = nullptr;

            if(commandQueue != nullptr){
                commandQueue->release();
                commandQueue = nullptr;
            }
            if(device != nullptr){
                device->release();
                device = nullptr;
            }

            // _objc_autoreleasePoolPrint();
        }

        RHIFrameScopeRAII CreateFrameScope(){
            return std::make_unique<MetalFrameScope>();
        }

        RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView name
        ){
            return std::make_unique<MetalBuffer>(*device, desc, name);
        }

        RHITextureRAII CreateTexture(
            const RHITextureCreateDesc& desc,
            StrView name
        ){
            return std::make_unique<MetalTexture>(*device, desc, name);
        }

        RHISamplerRAII CreateSampler(
            const RHISamplerState& desc
        ){
            return std::make_unique<MetalSampler>(*device, desc);
        }

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            StrView name
        ){
            if(std::get_if<RHILegacyFrontendDesc>(&desc.preRasterizer)){
                return std::make_unique<MetalGraphicsPipelineState>(*device, desc, name);
            }
            else{
                // TODO. use Mesh Shader
                throw std::runtime_error("Unimplemented Graphics Pipeline Frontend");
            }
        }

        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc& desc,
            StrView name
        ){
            return std::make_unique<MetalComputePipelineState>(*device, desc, name);
        }

        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc& desc
        ){
            return std::make_unique<MetalSwapchain>(*device, desc);
        }

        RHICommandListRAII CreateCommandList(){
            return std::make_unique<MetalCommandList>(commandQueue);
        }

        RHIFenceRAII CreateFence(u64 initialValue){
            return std::make_unique<MetalFence>(*device, initialValue);
        }

        void Submit(RHICommandList& cmdList);

        MTL::Device* Get() noexcept{ return device; }
        MetalCommandList& GetMainCmdList() noexcept{ return *cmdList; }
    };

    MetalDevice::MetalDevice()
        : impl(std::make_unique<Impl>()){}

    MetalDevice::~MetalDevice(){}

    RHIFrameScopeRAII MetalDevice::CreateFrameScope(){
        return impl->CreateFrameScope();
    }

    RHIBufferRAII MetalDevice::CreateBuffer(
        const RHIBufferCreateDesc& desc,
        StrView name
    ){
        return impl->CreateBuffer(desc, name);
    }

    RHITextureRAII MetalDevice::CreateTexture(
        const RHITextureCreateDesc& desc,
        StrView name
    ){
        return impl->CreateTexture(desc, name);
    }

    RHISamplerRAII MetalDevice::CreateSampler(
        const RHISamplerState& desc
    ){
        return impl->CreateSampler(desc);
    }

    RHIGraphicsPipelineStateRAII MetalDevice::CreatePipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        StrView name
    ){
        return impl->CreatePipelineState(desc, name);
    }

    RHIComputePipelineStateRAII MetalDevice::CreatePipelineState(
        const RHIComputePipelineStateDesc& desc,
        StrView name
    ){
        return impl->CreatePipelineState(desc, name);
    }

    RHISwapchainRAII MetalDevice::CreateSwapchain(
        const RHISwapchainCreateDesc& desc
    ){
        return impl->CreateSwapchain(desc);
    }

    RHICommandListRAII MetalDevice::CreateCommandList(){
        return impl->CreateCommandList();
    }

    RHIFenceRAII MetalDevice::CreateFence(u64 initialValue){
        return impl->CreateFence(initialValue);
    }

    void MetalDevice::SignalFence(
        RHICommandList& cmdList,
        RHIFence& fence,
        u64 value
    ){
        auto& mtlCmdList = static_cast<MetalCommandList&>(cmdList);
        auto& mtlFence = static_cast<MetalFence&>(fence);

        auto cmdBuf = mtlCmdList.Get();
        auto event = mtlFence.Get();

        cmdBuf->encodeSignalEvent(event, value);
    }

    RHICapabilities MetalDevice::GetCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void MetalDevice::Submit(RHICommandList& cmdList){
        impl->Submit(cmdList);
    }

    void MetalDevice::Impl::Submit(
        RHICommandList& cmdList
    ){
        auto& mtlCmdList = static_cast<MetalCommandList&>(cmdList);
        auto cmdBuffer = mtlCmdList.Get();
        cmdBuffer->commit();
    }

    void* MetalDevice::Get() noexcept{
        return impl->Get();
    }

    RHICommandList& MetalDevice::GetMainCmdList() noexcept{
        return impl->GetMainCmdList();
    }
}
