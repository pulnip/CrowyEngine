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
#include "MetalUtil.hpp"
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

        RAII<MetalCommandList> uploadCmdList;
        bool uploadRecorded = false;
        UploadRing uploadRing;

        AutoreleasePoolScope autoreleasePool;

        // increased on Submit
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
            CROWY_ASSERT(desc.size % 4 == 0);

            auto buffer = std::make_unique<MetalBuffer>(
                *device,
                desc,
                frameIndex,
                name
            );

            if(desc.initialData != nullptr && desc.access == RHIMemoryAccess::GPUOnly){
                ensureUpload();

                UploadGpuOnlyBuffer(
                    *uploadCmdList,
                    uploadRing,
                    4,
                    *buffer,
                    RHISubresourceData{
                        .data = desc.initialData,
                        .rowPitch = desc.size
                    }
                );
            }

            return buffer;
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
            auto texDesc = convert(desc);
            auto sizeAlign = device->heapTextureSizeAndAlign(texDesc);

            auto texture = std::make_unique<MetalTexture>(
                *device,
                texDesc,
                name
            );
            texDesc->release();

            if(!desc.initialData.empty()){
                ensureUpload();

                const usize n = desc.mipLevels * desc.arraySize;
                CROWY_ASSERT(desc.initialData.size() == n);

                std::vector<RHISubresourceLayout> layouts(n);
                const auto totalBytes = QueryUploadLayout(
                    desc,
                    layouts
                );
                UploadTexture(
                    *uploadCmdList,
                    uploadRing,
                    sizeAlign.align,
                    *texture,
                    desc.initialData,
                    layouts,
                    totalBytes
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
            auto cmdBuffer = commandQueue->commandBuffer();
            fence.Encode(*cmdBuffer, value);

            cmdBuffer->commit();
        }

        void Submit(
            std::span<RHICommandList*> cmdLists,
            MetalFence& fence
        ){
            commitUpload();

            auto lastCmdBuffer = static_cast<MetalCommandList*>(cmdLists.back())->Get();
            fence.Encode(*lastCmdBuffer, ++frameIndex);

            for(auto cmdList: cmdLists){
                auto mtlCmdList = static_cast<MetalCommandList*>(cmdList)->Get();
                mtlCmdList->commit();
            }
        }

        void SubmitAndPresent(
            std::span<RHICommandList*> cmdLists,
            MetalSwapchain& swapchain,
            MetalFence& fence
        ){
            commitUpload();

            auto lastCmdBuffer = static_cast<MetalCommandList&>(*cmdLists.back()).Get();
            swapchain.Present(*lastCmdBuffer);
            fence.Encode(*lastCmdBuffer, ++frameIndex);

            for(auto cmdList: cmdLists){
                auto mtlCmdList = static_cast<MetalCommandList*>(cmdList)->Get();
                mtlCmdList->commit();
            }
        }

        u64& GetFrameIndexRef() noexcept{
            return frameIndex;
        }

        MTL::Device* Get() noexcept{
            return device;
        }

        // Metal has no GetCopyableFootprints equivalent,
        // so compute the staging-buffer footprint by hand.
        u64 QueryUploadLayout(
            const RHITextureCreateDesc& desc,
            std::span<RHISubresourceLayout> out
        ){
            // copyFromBuffer requires sourceOffset to be a multiple of
            // the pixel size (block size for compressed formats);
            // 256 covers every format.
            constexpr u64 offsetAlign = 256;
            const u32 blockDim = GetBlockDim(desc.format);

            u64 totalBytes = 0;
            for(u32 slice = 0; slice < desc.arraySize; ++slice){
                for(u32 mip = 0; mip < desc.mipLevels; ++mip){
                    const u32 mipWidth  = std::max(1u, desc.width  >> mip);
                    const u32 mipHeight = std::max(1u, desc.height >> mip);

                    const u32 rowSize  = GetRowPitch(desc.format, mipWidth);
                    const u32 rowCount = ceilDiv(mipHeight, blockDim);

                    totalBytes = nextMul(totalBytes, offsetAlign);

                    // rowPitch == rowSize: Metal allows tight packing
                    out[slice * desc.mipLevels + mip] = RHISubresourceLayout{
                        .offset = totalBytes,
                        .rowPitch = rowSize,
                        .rowSize = rowSize,
                        .rowCount = rowCount
                    };
                    totalBytes += u64(rowSize) * rowCount;
                }
            }
            return totalBytes;
        }

    private:
        void ensureUpload(){
            if(!uploadRecorded){
                uploadCmdList->Begin();
                uploadCmdList->BeginBlit();
                uploadRecorded = true;
            }
        }

        void commitUpload(){
            if(uploadRecorded){
                uploadCmdList->EndBlit();
                uploadCmdList->Close();

                auto mtlCmdList = uploadCmdList->Get();
                mtlCmdList->commit();

                uploadRecorded = false;
            }
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
            .clipSpaceMinZ = 0.0f,
            // maximum pixel/block size of All RHIPixelFormat
            .textureRowPitchAlign = 16,
            .textureOffsetAlign = 16,
        };
    }

    void MetalDevice::Submit(
        std::span<RHICommandList*> cmdLists,
        RHIFence& fence
    ){
        impl->Submit(
            cmdLists,
            static_cast<MetalFence&>(fence)
        );
    }
    void MetalDevice::SubmitAndPresent(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain,
        RHIFence& fence
    ){
        impl->SubmitAndPresent(
            cmdLists,
            static_cast<MetalSwapchain&>(swapchain),
            static_cast<MetalFence&>(fence)
        );
    }

    void* MetalDevice::Get() noexcept{
        return impl->Get();
    }
}
