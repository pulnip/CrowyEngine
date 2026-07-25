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
#include "RHIShader.hpp"
#include "RHIUtil.hpp"
#include "UploadRing.hpp"

namespace Crowy
{
    RHIDeviceRAII CreateMetalDevice(){
        return std::make_unique<MetalDevice>();
    }

    class MetalDevice::Impl{
    private:
        MTL::Device* device;
        MTL::CommandQueue* commandQueue;
        MTL::CommandBuffer* finalCommandBuffer = nullptr;

        RAII<MetalCommandList> uploadCmdList;
        bool uploadRecorded = false;
        UploadRing uploadRing;

        AutoreleasePoolScope autoreleasePool;

        // increased by FramePacer
        u64 frameIndex = 0;

    public:
        auto CreateCommandList(){
            return std::make_unique<MetalCommandList>(
                commandQueue
            );
        }

        auto CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView name
        ){
            return std::make_unique<MetalBuffer>(
                *device,
                desc,
                frameIndex,
                name
            );
        }

        Impl()
            : device(MTL::CreateSystemDefaultDevice())
            , commandQueue(device->newCommandQueue())
        {
            CROWY_ASSERT(device != nullptr, "No GPU Available");
            CROWY_ASSERT(commandQueue != nullptr,
                "Failed to create command queue"
            );

            InitGlobalSession();

            uploadCmdList = CreateCommandList();

            auto stagingBuffer = CreateBuffer(
                RHIBufferCreateDesc{
                    .size = 1 << 25,
                    .usage = RHIBufferUsage::CopySrc,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = nullptr
                }, "staging buffer"
            );
            uploadRing = UploadRing(std::move(stagingBuffer));
        }

        ~Impl(){
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

        auto CreateFrameScope(){
            return std::make_unique<MetalFrameScope>();
        }

        auto CreateTexture(
            const RHITextureCreateDesc& desc,
            StrView name
        ){
            using enum RHIMemoryAccess;

            CROWY_ASSERT(desc.access != CPUWrite && desc.access != CPURead,
                "Use RHIBuffer for CPU-Accessable Resource"
            );

            auto texture = std::make_unique<MetalTexture>(
                *device,
                desc,
                name
            );
            if(!desc.initialData.empty()){
                if(!uploadRecorded){
                    uploadCmdList->Begin();
                    uploadCmdList->BeginBlit();
                    uploadRecorded = true;
                }

                const usize n = desc.mipLevels * desc.arraySize;
                CROWY_ASSERT(desc.initialData.size() == n);

                std::vector<RHISubresourceLayout> layouts(n);
                UploadTexture(
                    *uploadCmdList,
                    uploadRing,
                    -1,
                    *texture,
                    desc.initialData,
                    layouts,
                    -1
                );
            }

            return texture;
        }

        auto CreateSampler(
            const RHISamplerState& desc
        ){
            return std::make_unique<MetalSampler>(
                *device,
                desc
            );
        }

        auto CreatePipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            StrView name
        ){
            if(std::get_if<RHILegacyFrontendDesc>(&desc.preRasterizer)){
                return std::make_unique<MetalGraphicsPipelineState>(
                    *device,
                    desc,
                    name
                );
            }
            else{
                throw std::runtime_error("Unimplemented");
            }
        }

        auto CreatePipelineState(
            const RHIComputePipelineStateDesc& desc,
            StrView name
        ){
            return std::make_unique<MetalComputePipelineState>(
                *device,
                desc,
                name
            );
        }

        auto CreateSwapchain(
            const RHISwapchainCreateDesc& desc
        ){
            return std::make_unique<MetalSwapchain>(
                *device,
                desc
            );
        }

        auto CreateFence(u64 initialValue){
            return std::make_unique<MetalFence>(
                *device,
                initialValue
            );
        }

        void SignalFence(MetalFence& fence, u64 value){
            finalCommandBuffer = commandQueue->commandBuffer();
            auto sharedEvent = fence.Get();

            finalCommandBuffer->encodeSignalEvent(sharedEvent, value);
        }

        void Submit(std::span<RHICommandList*> cmdLists){
            if(uploadRecorded){
                uploadCmdList->EndBlit();
                uploadCmdList->Close();

                auto mtlCmdList = uploadCmdList->Get();
                mtlCmdList->commit();

                uploadRecorded = false;
            }

            for(auto cmdList: cmdLists){
                auto mtlCmdList = static_cast<MetalCommandList*>(cmdList)->Get();
                mtlCmdList->commit();
            }

            finalCommandBuffer->commit();
        }

        void SubmitAndPresent(
            std::span<RHICommandList*> cmdLists,
            MetalSwapchain& swapchain
        ){
            if(uploadRecorded){
                uploadCmdList->EndBlit();
                uploadCmdList->Close();

                auto mtlCmdList = uploadCmdList->Get();
                mtlCmdList->commit();

                uploadRecorded = false;
            }

            for(auto cmdList: cmdLists){
                auto mtlCmdList = static_cast<MetalCommandList*>(cmdList)->Get();
                mtlCmdList->commit();
            }

            swapchain.Present(*finalCommandBuffer);
            finalCommandBuffer->commit();
        }

        u64& GetFrameIndexRef() noexcept{
            return frameIndex;
        }

        MTL::Device* Get() noexcept{
            return device;
        }
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
        const RHISwapchainCreateDesc& desc,
        StrView
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
        RHIFence& fence,
        u64 value
    ){
        impl->SignalFence(static_cast<MetalFence&>(fence), value);
    }

    u64& MetalDevice::GetFrameIndexRef() noexcept{
        return impl->GetFrameIndexRef();
    }

    RHICapabilities MetalDevice::GetCapabilities() const noexcept{
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f
        };
    }

    void MetalDevice::Submit(std::span<RHICommandList*> cmdLists){
        impl->Submit(cmdLists);
    }
    void MetalDevice::SubmitAndPresent(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain
    ){
        impl->SubmitAndPresent(
            cmdLists,
            static_cast<MetalSwapchain&>(swapchain)
        );
    }

    void* MetalDevice::Get() noexcept{
        return impl->Get();
    }
}
