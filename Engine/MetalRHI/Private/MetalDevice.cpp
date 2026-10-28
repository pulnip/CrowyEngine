#define NS_PRIVATE_IMPLEMENTATION
#define MTL_PRIVATE_IMPLEMENTATION
#define CA_PRIVATE_IMPLEMENTATION
#include "MetalDevice.hpp"

#include <algorithm>
#include <vector>

#include <Metal/MTLFence.hpp>

#include "Assert.hpp"
#include "AutoreleasePoolScope.hpp"
#include "MetalAllocator.hpp"
#include "MetalBuffer.hpp"
#include "MetalCommandList.hpp"
#include "MetalFence.hpp"
#include "MetalFrameScope.hpp"
#include "MetalHeapPool.hpp"
#include "MetalPipelineState.hpp"
#include "MetalSampler.hpp"
#include "MetalSwapchain.hpp"
#include "MetalTexture.hpp"
#include "MetalUtil.hpp"
#include "RHIFrameTimeline.hpp"
#include "RHIGPUTime.hpp"
#include "RHIRetireQueue.hpp"
#include "RHIShader.hpp"
#include "RHISubmitLedger.hpp"
#include "UploadRing.hpp"

namespace Crowy
{
    RHIDeviceRAII createMetalDevice() {
        return std::make_unique<MetalDevice>();
    }

    class MetalDevice::Impl {
    private:
        // a submitted frame's command buffers, held past their lists' next
        // Begin until the frame completes and its GPU time is read back
        struct UntimedFrame {
            u64 frame = 0;
            std::vector<NS::SharedPtr<MTL::CommandBuffer>> cmdBuffers;
        };

        NS::SharedPtr<MTL::Device> device;
        NS::SharedPtr<MTL::CommandQueue> commandQueue;
        // every wave's last command buffer signals here; a recording that
        // depends on prior waves waits on it, restoring queue-global sync
        NS::SharedPtr<MTL::Event> submissionEvent;

        MetalReservedSamplers samplers;

        MetalHeapPool privateHeap;
        MetalHeapPool sharedHeap;

        // every resource below is allocated through this, so it has to
        // outlive them - the staging buffer inside uploadRing included
        MetalAllocator allocator;

        AutoreleasePoolScope autoreleasePool;

        MetalCommandList uploadCmdList;
        bool uploadRecorded = false;
        // creation staging: marked whenever the upload list goes out, with a
        // submitted wave or through flushUploads
        UploadRing uploadRing;
        // AllocateTransient's staging: marked only when a wave is submitted,
        // so a flush never retires a slice an open recording still reads
        UploadRing transientRing;

        // the device's one timeline: every submitted wave and every out-of-band
        // signal takes the next value, on serialFence and submissionEvent alike
        u64 submissionSerial = 0;
        // serial of the last wave that left un-acquired hand-off releases
        u64 handoffSerial = 0;

        RAII<MetalFence> serialFence;
        RHIFrameTimeline timeline;
        RHIRetireQueue retireQueue;
        RHISubmitLedger ledger;

        // in submission order; the upload list is never among them
        std::vector<UntimedFrame> untimedFrames;
        RHIGPUTimeHistory gpuTimes;

    public:
        auto CreateCommandList() {
            return std::make_unique<MetalCommandList>(
                commandQueue.get(),
                submissionEvent.get(),
                submissionSerial,
                handoffSerial
            );
        }

        auto CreateBuffer(const RHIBufferCreateDesc& desc, StrView name) {
            CROWY_ASSERT(desc.size % 4 == 0);

            auto buffer = std::make_unique<MetalBuffer>(allocator, desc, name);

            if(desc.initialData != nullptr &&
               desc.memory == RHIMemoryType::GPUOnly) {
                ensureUploadBegin();

                uploadGPUOnlyBuffer(
                    uploadCmdList,
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
            : device(NS::TransferPtr(MTL::CreateSystemDefaultDevice())),
              commandQueue(NS::TransferPtr(device->newCommandQueue())),
              submissionEvent(NS::TransferPtr(device->newEvent())),
              samplers(*device.get()),
              privateHeap(
                  *device.get(),
                  {.heapSize = 128ull << 20},
                  "PrivateHeap"
              ),
              sharedHeap(
                  *device.get(),
                  {.storageMode = MTL::StorageModeShared,
                   .heapSize = 128ull << 20},
                  "SharedHeap"
              ),
              allocator(privateHeap, sharedHeap)
              // binds references to members that initialize later - fine,
              // the command list only reads them inside Begin()
              ,
              uploadCmdList(
                  commandQueue.get(),
                  submissionEvent.get(),
                  submissionSerial,
                  handoffSerial
              ),
              serialFence(std::make_unique<MetalFence>(*device.get(), 0)),
              ledger(
                  *serialFence,
                  retireQueue,
                  uploadRing,
                  transientRing,
                  timeline
              ) {
            CROWY_ASSERT(device, "No GPU Available");
            CROWY_ASSERT(commandQueue, "Failed to create command queue");

#if defined(_DEBUG) || !defined(NDEBUG)
            submissionEvent->setLabel(toNSString("Crowy Submission Event"));
#endif

            initGlobalSession();

            commandQueue->addResidencySet(privateHeap.ResidencySet());
            commandQueue->addResidencySet(sharedHeap.ResidencySet());

            auto stagingBuffer = CreateBuffer(
                RHIBufferCreateDesc{
                    .size = UploadStagingBytes,
                    .memory = RHIMemoryType::CPUWrite
                },
                "upload staging"
            );
            uploadRing =
                UploadRing(*serialFence, std::move(stagingBuffer), [this] {
                    flushUploads();
                });

            // no flush hook: only a submitted wave may retire a transient, so
            // running out means the size is wrong, not that work is pending
            auto transientBuffer = CreateBuffer(
                RHIBufferCreateDesc{
                    .size = TransientStagingBytes,
                    .memory = RHIMemoryType::CPUWrite
                },
                "transient staging"
            );
            transientRing =
                UploadRing(*serialFence, std::move(transientBuffer));
        }

        ~Impl() { retireQueue.CollectAll(); }

        auto CreateFrameScope() { return std::make_unique<MetalFrameScope>(); }

        auto CreateTexture(const RHITextureCreateDesc& desc, StrView name) {
            auto texDesc = convert(desc);
            auto sizeAlign = device->heapTextureSizeAndAlign(texDesc);

            auto texture =
                std::make_unique<MetalTexture>(allocator, texDesc, name);
            texDesc->release();

            if(!desc.initialData.empty()) {
                // RHISubresourceData carries no slice pitch,
                // so the upload helpers only understand 2D subresources
                CROWY_ASSERT(
                    desc.depth == 1,
                    "initial data upload is not supported for 3D textures"
                );
                ensureUploadBegin();

                const usize n = desc.mipLevels * desc.arraySize;
                CROWY_ASSERT(desc.initialData.size() == n);

                std::vector<RHISubresourceLayout> layouts(n);
                const auto totalBytes = QueryUploadLayout(desc, layouts);
                uploadTexture(
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

        auto CreateSampler(const RHISamplerState& desc) {
            return std::make_unique<MetalSampler>(*device.get(), desc);
        }

        auto CreatePipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            StrView name
        ) {
            return std::make_unique<MetalGraphicsPipelineState>(
                *device.get(),
                samplers,
                desc,
                name
            );
        }

        auto CreatePipelineState(
            const RHIComputePipelineStateDesc& desc,
            StrView name
        ) {
            return std::make_unique<MetalComputePipelineState>(
                *device.get(),
                samplers,
                desc,
                name
            );
        }

        auto CreateSwapchain(const RHISwapchainCreateDesc& desc) {
            return std::make_unique<MetalSwapchain>(*device.get(), desc);
        }

        void Submit(std::span<RHICommandList*> cmdLists, u64 frame) {
            submitFrame(cmdLists, frame, [](MTL::CommandBuffer&) {});
        }

        void SubmitAndPresent(
            std::span<RHICommandList*> cmdLists,
            MetalSwapchain& swapchain,
            u64 frame
        ) {
            submitFrame(
                cmdLists,
                frame,
                [&](MTL::CommandBuffer& lastCmdBuffer) {
                    swapchain.Present(lastCmdBuffer, frame);
                }
            );
        }

        u64 GetCompletedFrame() const noexcept {
            return timeline.CompletedFrame(serialFence->GetValue());
        }

        std::optional<f64> GetGPUFrameTime(u64 frame) const noexcept {
            if(const auto* seconds = gpuTimes.Find(frame))
                return *seconds;

            return std::nullopt;
        }

        void WaitFrame(u64 frame) {
            if(const auto serial = timeline.SerialOf(frame))
                serialFence->WaitCPU(*serial, 0);
        }

        void WaitIdle() {
            // signal fresh so this also waits on any GPU work queued
            // after the last per-frame signal (e.g. swapchain Present)
            signalSerial();
            serialFence->WaitCPU(submissionSerial, 0);

            // all work up to the fresh signal is done, so this drains the
            // queue instead of leaving stragglers for a Submit
            retireQueue.Tag(submissionSerial);
            retireQueue.Collect(submissionSerial);

            // the event can fire before a buffer's status says Completed, and
            // nothing after the loop's last WaitIdle would read them again
            for(const auto& held: untimedFrames) {
                for(const auto& cmdBuffer: held.cmdBuffers)
                    cmdBuffer->waitUntilCompleted();
            }
            harvestGPUTimes();
        }

        void DeferRetire(std::move_only_function<void()> reclaim) {
            retireQueue.Defer(std::move(reclaim));
        }

        RHIBufferSlice AllocateTransient(u32 size, u32 align) {
            const auto alloc = transientRing.Allocate(size, align);

            return RHIBufferSlice{
                .buffer = &alloc.buffer,
                .offset = static_cast<u32>(alloc.offset),
                .size = size,
                .cpuPtr = alloc.cpuPtr
            };
        }

        // Metal has no GetCopyableFootprints equivalent,
        // so compute the staging-buffer footprint by hand.
        u64 QueryUploadLayout(
            const RHITextureCreateDesc& desc,
            std::span<RHISubresourceLayout> out
        ) {
            // copyFromBuffer needs sourceOffset on a pixel or block size
            // multiple; 256 covers every format
            constexpr u64 offsetAlign = 256;
            const u32 blockDim = getBlockDim(desc.format);

            u64 totalBytes = 0;
            for(u32 slice = 0; slice < desc.arraySize; ++slice) {
                for(u32 mip = 0; mip < desc.mipLevels; ++mip) {
                    const u32 mipWidth = std::max(1u, desc.width >> mip);
                    const u32 mipHeight = std::max(1u, desc.height >> mip);

                    const u32 rowSize = getRowPitch(desc.format, mipWidth);
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
        template<typename Present>
        void submitFrame(
            std::span<RHICommandList*> cmdLists,
            u64 frame,
            Present present
        ) {
            const auto completed = ledger.Collect();
            harvestGPUTimes();

            ensureUploadCommit();

            auto lastCmdBuffer =
                static_cast<MetalCommandList*>(cmdLists.back())->Get();
            present(*lastCmdBuffer);
            serialFence->Encode(*lastCmdBuffer, ++submissionSerial);
            // close the wave: later recordings that depend on it
            // (lazy gates) wait on this value
            lastCmdBuffer->encodeSignalEvent(
                submissionEvent.get(),
                submissionSerial
            );
            ledger.Stamp(frame, submissionSerial, completed);

            trackHandoffs(cmdLists);

            for(auto cmdList: cmdLists) {
                auto mtlCmdList =
                    static_cast<MetalCommandList*>(cmdList)->Get();
                mtlCmdList->commit();
            }
            holdForGPUTime(cmdLists, frame);
        }

        void holdForGPUTime(std::span<RHICommandList*> cmdLists, u64 frame) {
            UntimedFrame held{.frame = frame};
            held.cmdBuffers.reserve(cmdLists.size());
            for(auto cmdList: cmdLists) {
                held.cmdBuffers.push_back(
                    NS::RetainPtr(
                        static_cast<MetalCommandList*>(cmdList)->Get()
                    )
                );
            }

            untimedFrames.push_back(std::move(held));
        }

        // read here, on the submitting thread, once every command buffer of a
        // frame completes; a frame that failed on the GPU gets no time
        void harvestGPUTimes() {
            usize harvested = 0;
            for(const auto& held: untimedFrames) {
                const bool completed = std::ranges::all_of(
                    held.cmdBuffers,
                    [](const NS::SharedPtr<MTL::CommandBuffer>& cmdBuffer) {
                        return cmdBuffer->status() >=
                               MTL::CommandBufferStatusCompleted;
                    }
                );
                // one queue completes in order, so no later frame has either
                if(!completed)
                    break;

                std::vector<RHIGPUInterval> intervals;
                intervals.reserve(held.cmdBuffers.size());
                bool failed = false;
                for(const auto& cmdBuffer: held.cmdBuffers) {
                    failed |=
                        cmdBuffer->status() == MTL::CommandBufferStatusError;
                    intervals.push_back(
                        RHIGPUInterval{
                            .begin = cmdBuffer->GPUStartTime(),
                            .end = cmdBuffer->GPUEndTime()
                        }
                    );
                }
                if(!failed)
                    gpuTimes.Push(held.frame, busySeconds(intervals));

                ++harvested;
            }

            untimedFrames.erase(
                untimedFrames.begin(),
                untimedFrames.begin() + static_cast<std::ptrdiff_t>(harvested)
            );
        }

        // out-of-band signal (WaitIdle, flushUploads) that is not tied to a
        // submitted wave, so it needs its own command buffer to carry it
        void signalSerial() {
            auto cmdBuffer = commandQueue->commandBuffer();
            serialFence->Encode(*cmdBuffer, ++submissionSerial);
            cmdBuffer->encodeSignalEvent(
                submissionEvent.get(),
                submissionSerial
            );

            cmdBuffer->commit();
        }

        // a wave that leaves unacquired hand-offs gates recordings that may
        // overlap it; the upload list's are closed by ensureUploadCommit's wait
        void trackHandoffs(std::span<RHICommandList*> cmdLists) {
            for(auto cmdList: cmdLists) {
                if(static_cast<MetalCommandList*>(cmdList)
                       ->HasUnconsumedReleases()) {
                    handoffSerial = submissionSerial;
                    return;
                }
            }
        }

        void ensureUploadBegin() {
            if(!uploadRecorded) {
                uploadCmdList.Begin();
                uploadRecorded = true;
            }
        }

        void ensureUploadCommit() {
            if(uploadRecorded) {
                uploadCmdList.Close();

                auto mtlCmdList = uploadCmdList.Get();
                mtlCmdList->commit();
                // the wave's consumers were recorded before this buffer, so the
                // event cannot order them; this is load-time work, so CPU-wait
                mtlCmdList->waitUntilCompleted();

                uploadRecorded = false;
            }
        }

        // the upload ring's escape hatch, called inside UploadRing::Allocate
        // before its caller records; the fresh signal gives the ring a serial
        void flushUploads() {
            if(!uploadRecorded)
                return;

            ensureUploadCommit();

            signalSerial();
            uploadRing.OnSubmit(submissionSerial);

            ensureUploadBegin();
        }
    };

    MetalDevice::MetalDevice() = default;

    MetalDevice::~MetalDevice() = default;

    RHIFrameScopeRAII MetalDevice::CreateFrameScope() {
        return impl->CreateFrameScope();
    }

    RHIBufferRAII MetalDevice::createBuffer(
        const RHIBufferCreateDesc& desc,
        StrView name
    ) {
        return impl->CreateBuffer(desc, name);
    }

    RHITextureRAII MetalDevice::createTexture(
        const RHITextureCreateDesc& desc,
        StrView name
    ) {
        return impl->CreateTexture(desc, name);
    }

    RHIGraphicsPipelineStateRAII MetalDevice::CreatePipelineState(
        const RHIGraphicsPipelineStateDesc& desc,
        StrView name
    ) {
        return impl->CreatePipelineState(desc, name);
    }

    RHIComputePipelineStateRAII MetalDevice::CreatePipelineState(
        const RHIComputePipelineStateDesc& desc,
        StrView name
    ) {
        return impl->CreatePipelineState(desc, name);
    }

    RHISwapchainRAII MetalDevice::CreateSwapchain(
        const RHISwapchainCreateDesc& desc,
        StrView
    ) {
        return impl->CreateSwapchain(desc);
    }

    RHICommandListRAII MetalDevice::CreateCommandList() {
        return impl->CreateCommandList();
    }
    RHICapabilities MetalDevice::GetCapabilities() const noexcept {
        return {
            .flipTextureV = true,
            .clipSpaceMinZ = 0.0f,
            // maximum pixel/block size of All RHIPixelFormat
            .textureRowPitchAlign = 16,
            .textureOffsetAlign = 16,
        };
    }

    void MetalDevice::submit(std::span<RHICommandList*> cmdLists, u64 frame) {
        impl->Submit(cmdLists, frame);
    }

    void MetalDevice::submitAndPresent(
        std::span<RHICommandList*> cmdLists,
        RHISwapchain& swapchain,
        u64 frame
    ) {
        impl->SubmitAndPresent(
            cmdLists,
            static_cast<MetalSwapchain&>(swapchain),
            frame
        );
    }

    u64 MetalDevice::GetCompletedFrame() const noexcept {
        return impl->GetCompletedFrame();
    }

    std::optional<f64> MetalDevice::GetGPUFrameTime(u64 frame) const noexcept {
        return impl->GetGPUFrameTime(frame);
    }

    void MetalDevice::WaitFrame(u64 frame) {
        impl->WaitFrame(frame);
    }

    void MetalDevice::WaitIdle() {
        impl->WaitIdle();
    }

    void MetalDevice::DeferRetire(std::move_only_function<void()> reclaim) {
        impl->DeferRetire(std::move(reclaim));
    }

    RHIBufferSlice MetalDevice::AllocateTransient(u32 size, u32 align) {
        return impl->AllocateTransient(size, align);
    }
}
