extern "C"{
    // Debug AutoreleasePool
    void _objc_autoreleasePoolPrint(void);
}

#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include <Metal/MTLFence.hpp>
#include "Assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalDevice.hpp"
#include "MetalFence.hpp"
#include "MetalFrameScope.hpp"
#include "MetalHeapPool.hpp"
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
        NS::SharedPtr<MTL::Device> device;
        NS::SharedPtr<MTL::CommandQueue> commandQueue;
        // cross-submission ordering:
        // the last command buffer of every submitted wave signals here, and
        // recordings that depend on prior waves (cross-submission acquires,
        // the hand-off hangover window) lazily wait on the last signaled
        // value. this restores the queue-global sync scope the barrier
        // contract assumes, without giving up inter-wave GPU overlap
        // for recordings that declare no such dependency.
        NS::SharedPtr<MTL::Event> submissionEvent;

        MetalReservedSamplers samplers;

        MetalHeapPool privateHeap;
        MetalHeapPool sharedHeap;
        // MetalHeapPool memorylessHeap;

        AutoreleasePoolScope autoreleasePool;

        MetalCommandList uploadCmdList;
        bool uploadRecorded = false;
        UploadRing uploadRing;

        // increased on Submit
        u64 frameIndex = 0;
        // last value actually signaled on submissionEvent.
        // distinct from frameIndex, which callers may bump out-of-band without a signal
        // (FramePacer::WaitForIdle, helloCompute's slot skip)
        // - a lazy gate must never wait a value nothing signals
        u64 submissionSerial = 0;
        // serial of the last wave that left un-acquired hand-off releases
        u64 handoffSerial = 0;

    public:
        auto CreateCommandList(){
            return std::make_unique<MetalCommandList>(
                commandQueue.get(),
                submissionEvent.get(),
                submissionSerial,
                handoffSerial
            );
        }

        auto CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView name
        ){
            CROWY_ASSERT(desc.size % 4 == 0);

            auto buffer = std::make_unique<MetalBuffer>(
                desc.access == RHIMemoryAccess::GPUOnly ?
                    privateHeap : sharedHeap,
                desc,
                frameIndex,
                name
            );

            if(desc.initialData != nullptr && desc.access == RHIMemoryAccess::GPUOnly){
                ensureUploadBegin();

                UploadGpuOnlyBuffer(
                    uploadCmdList,
                    uploadRing,
                    4,
                    *buffer,
                    desc.usage,
                    RHISubresourceData{
                        .data = desc.initialData,
                        .rowPitch = desc.size
                    }
                );
            }

            return buffer;
        }

        Impl()
            : device(NS::TransferPtr(MTL::CreateSystemDefaultDevice()))
            , commandQueue(NS::TransferPtr(device->newCommandQueue()))
            , submissionEvent(NS::TransferPtr(device->newEvent()))
            , samplers(*device.get())
            , privateHeap(
                *device.get(), {
                    .heapSize = 128ull << 20
                },
                "PrivateHeap"
            )
            , sharedHeap(
                *device.get(), {
                    .storageMode = MTL::StorageModeShared,
                    .heapSize = 128ull << 20
                },
                "SharedHeap"
            )
            // binds references to members that initialize later - fine,
            // the command list only reads them inside Begin()
            , uploadCmdList(
                commandQueue.get(),
                submissionEvent.get(),
                submissionSerial,
                handoffSerial
            )
        {

            CROWY_ASSERT(device, "No GPU Available");
            CROWY_ASSERT(commandQueue,
                "Failed to create command queue"
            );

        #if defined(_DEBUG) || !defined(NDEBUG)
            submissionEvent->setLabel(toNSString("Crowy Submission Event"));
        #endif

            InitGlobalSession();

            commandQueue->addResidencySet(privateHeap.ResidencySet());
            commandQueue->addResidencySet(sharedHeap.ResidencySet());

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
                privateHeap,
                texDesc,
                name
            );
            texDesc->release();

            if(!desc.initialData.empty()){
                // RHISubresourceData carries no slice pitch,
                // so the upload helpers only understand 2D subresources
                CROWY_ASSERT(desc.depth == 1,
                    "initial data upload is not supported for 3D textures"
                );
                ensureUploadBegin();

                const usize n = desc.mipLevels * desc.arraySize;
                CROWY_ASSERT(desc.initialData.size() == n);

                std::vector<RHISubresourceLayout> layouts(n);
                const auto totalBytes = QueryUploadLayout(
                    desc,
                    layouts
                );
                UploadTexture(
                    uploadCmdList,
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
                *device.get(),
                desc
            );
        }

        auto CreatePipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            StrView name
        ){
            if(std::get_if<RHILegacyFrontendDesc>(&desc.preRasterizer)){
                return std::make_unique<MetalGraphicsPipelineState>(
                    *device.get(),
                    samplers,
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
                *device.get(),
                samplers,
                desc,
                name
            );
        }

        auto CreateSwapchain(
            const RHISwapchainCreateDesc& desc
        ){
            return std::make_unique<MetalSwapchain>(
                *device.get(),
                desc
            );
        }

        auto CreateFence(u64 initialValue){
            return std::make_unique<MetalFence>(
                *device.get(),
                initialValue
            );
        }

        void SignalFence(MetalFence& fence, u64 value){
            auto cmdBuffer = commandQueue->commandBuffer();
            fence.Encode(*cmdBuffer, value);
            // keep the submission event in step with out-of-band
            // frameIndex bumps (FramePacer::WaitForIdle signals through
            // here) so lazy gates never overtake the event timeline
            if(value > submissionSerial){
                cmdBuffer->encodeSignalEvent(submissionEvent.get(), value);
                submissionSerial = value;
            }

            cmdBuffer->commit();
        }

        void Submit(
            std::span<RHICommandList*> cmdLists,
            MetalFence& fence
        ){
            ensureUploadCommit();

            auto lastCmdBuffer = static_cast<MetalCommandList*>(cmdLists.back())->Get();
            fence.Encode(*lastCmdBuffer, ++frameIndex);
            // close the wave: later recordings that depend on it
            // (lazy gates) wait on this value
            submissionSerial = frameIndex;
            lastCmdBuffer->encodeSignalEvent(submissionEvent.get(), submissionSerial);

            trackHandoffs(cmdLists);

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
            ensureUploadCommit();

            auto lastCmdBuffer = static_cast<MetalCommandList&>(*cmdLists.back()).Get();
            swapchain.Present(*lastCmdBuffer);
            fence.Encode(*lastCmdBuffer, ++frameIndex);
            submissionSerial = frameIndex;
            lastCmdBuffer->encodeSignalEvent(submissionEvent.get(), submissionSerial);

            trackHandoffs(cmdLists);

            for(auto cmdList: cmdLists){
                auto mtlCmdList = static_cast<MetalCommandList*>(cmdList)->Get();
                mtlCmdList->commit();
            }
        }

        u64& GetFrameIndexRef() noexcept{
            return frameIndex;
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
        // a wave that leaves un-acquired hand-off releases gates the
        // recordings that may share the GPU with it
        // (hangover window in MetalCommandList::Begin).
        // the upload list is not scanned
        // - its hand-offs are already closed by the CPU wait in ensureUploadCommit
        void trackHandoffs(std::span<RHICommandList*> cmdLists){
            for(auto cmdList: cmdLists){
                if(static_cast<MetalCommandList*>(cmdList)->HasUnconsumedReleases()){
                    handoffSerial = submissionSerial;
                    return;
                }
            }
        }

        void ensureUploadBegin(){
            if(!uploadRecorded){
                uploadCmdList.Begin();
                uploadRecorded = true;
            }
        }

        void ensureUploadCommit(){
            if(uploadRecorded){
                uploadCmdList.Close();

                auto mtlCmdList = uploadCmdList.Get();
                mtlCmdList->commit();
                // the upload's hand-off releases are consumed by
                // command buffers of this very wave,
                // but those were recorded before this buffer existed,
                // so the submission event cannot order them behind it.
                // CPU-wait instead: creation uploads are load-time work
                mtlCmdList->waitUntilCompleted();

                uploadRecorded = false;
            }
        }
    };

    MetalDevice::MetalDevice() = default;
    MetalDevice::~MetalDevice() = default;

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
}
