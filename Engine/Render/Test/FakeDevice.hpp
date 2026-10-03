#pragma once

#include <cstddef>
#include <cstring>
#include <exception>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <vector>

#include "Assert.hpp"
#include "Function.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    // counts its own destruction, so a test can tell retired from destroyed
    class FakeGraphicsPipelineState final: public RHIGraphicsPipelineState {
    private:
        u32& destroyed;

    public:
        ~FakeGraphicsPipelineState() override { ++destroyed; }
        CROWY_DECLARE_PINNED(FakeGraphicsPipelineState)

        explicit FakeGraphicsPipelineState(u32& destroyed)
            : destroyed(destroyed) {}
    };

    // counts its own destruction, so a test can tell retired from destroyed
    class FakeComputePipelineState final: public RHIComputePipelineState {
    private:
        u32& destroyed;

    public:
        ~FakeComputePipelineState() override { ++destroyed; }
        CROWY_DECLARE_PINNED(FakeComputePipelineState)

        explicit FakeComputePipelineState(u32& destroyed)
            : destroyed(destroyed) {}
    };

    // CPU memory standing in for a buffer, so a test can read back what was
    // uploaded into it
    class FakeBuffer final: public RHIBuffer {
    public:
        using Bytes = std::vector<std::byte>;
        using RHIBuffer::GetReadableID;
        using RHIBuffer::GetWritableID;

        // the id every readable view of this buffer answers
        static constexpr u64 ReadableID = 0xB0FF;

        Bytes bytes;
        // what every writable view answers; 0 for a buffer that has none
        u64 writableID = 0;

        explicit FakeBuffer(u32 size, u64 writableID = 0)
            : bytes(size),
              writableID(writableID) {}

        void Upload(const void* data, u32 size, u32 offset) override {
            CROWY_ASSERT(offset + size <= bytes.size());

            std::memcpy(bytes.data() + offset, data, size);
        }
        void Download(void* data, u32 size, u32 offset) override {
            CROWY_ASSERT(offset + size <= bytes.size());

            std::memcpy(data, bytes.data() + offset, size);
        }
        u32 GetSize() const noexcept override {
            return static_cast<u32>(bytes.size());
        }
        void* GetMappedPtr() noexcept override { return bytes.data(); }
        u64 GetReadableID(const RHIBufferViewDesc&) override {
            return ReadableID;
        }
        u64 GetWritableID(const RHIBufferViewDesc&) override {
            CROWY_ASSERT(writableID != 0, "a buffer made without shaderWrite");

            return writableID;
        }

        // the element at this byte offset
        template<typename T>
            requires std::is_trivially_copyable_v<T>
        T Read(u32 offset) const {
            CROWY_ASSERT(offset + sizeof(T) <= bytes.size());

            T value;
            std::memcpy(&value, bytes.data() + offset, sizeof(T));

            return value;
        }
    };

    // a size, a format and an id, and nothing behind them; counts its own
    // destruction, so a test can tell retired from destroyed
    class FakeTexture final: public RHITexture {
    private:
        u32 width = 0;
        u32 height = 0;
        u64 readableID = 0;
        u32* destroyed = nullptr;

    public:
        static constexpr u64 WritableBit = 1ull << 63;

        using RHITexture::GetReadableID;
        using RHITexture::GetWritableID;

        ~FakeTexture() override {
            if(destroyed != nullptr)
                ++*destroyed;
        }
        CROWY_DECLARE_PINNED(FakeTexture)

        FakeTexture(
            RHIPixelFormat format,
            u32 width,
            u32 height,
            u64 readableID,
            u32* destroyed = nullptr
        )
            : RHITexture(format),
              width(width),
              height(height),
              readableID(readableID),
              destroyed(destroyed) {}

        u32 GetWidth() const noexcept override { return width; }
        u32 GetHeight() const noexcept override { return height; }
        u32 GetDepth() const noexcept override { return 1; }
        u64 GetReadableID(const RHITextureViewDesc&) override {
            return readableID;
        }
        // the readable id with the top bit set
        u64 GetWritableID(const RHITextureViewDesc&) override {
            return readableID | WritableBit;
        }
        void* GetNative() noexcept override { return nullptr; }
    };

    // compiles nothing: graphics pipelines are counted fakes, and a
    // deferred retire waits in `deferred` until the test runs it; transient
    // slices come from one CPU buffer that never moves; textures are fakes
    // that remember their descs, and buffers CPU memory
    class FakeDevice final: public RHIDevice {
    public:
        using Reclaims = std::vector<std::move_only_function<void()>>;
        using PipelineCreates = std::vector<RHIGraphicsPipelineStateDesc>;
        using ComputeCreates = std::vector<RHIComputePipelineStateDesc>;
        using TextureCreates = std::vector<RHITextureCreateDesc>;
        using BufferCreates = std::vector<RHIBufferCreateDesc>;

        static constexpr u32 TransientSize = 1u << 20;
        // a texture's readable id is this plus its create's index
        static constexpr u64 FirstTextureID = 0x7E00;
        // a shader-writable buffer's writable id is this plus its create's
        // index
        static constexpr u64 FirstWritableBufferID = 0xB100;
        // D3D12's copy footprint rules
        static constexpr RHICapabilities Capabilities{
            .flipTextureV = false,
            .clipSpaceMinZ = 0.0f,
            .textureRowPitchAlign = 256,
            .textureOffsetAlign = 512
        };

        u32 creates = 0;
        // the create that throws, counted from 1; 0 never throws
        u32 failAt = 0;
        u32 destroyed = 0;
        // every graphics pipeline's desc, in create order
        PipelineCreates pipelineCreates;
        // every compute pipeline's desc, in create order; `creates` and
        // `failAt` count both kinds
        ComputeCreates computeCreates;
        u32 computeDestroyed = 0;
        TextureCreates textureCreates;
        u32 texturesDestroyed = 0;
        BufferCreates bufferCreates;
        // what GetCompletedFrame answers
        u64 completedFrame = 0;
        Reclaims deferred;
        FakeBuffer transient{TransientSize};
        // bytes handed out, and how many slices
        u32 transientUsed = 0;
        u32 transientAllocations = 0;

        RHIFrameScopeRAII CreateFrameScope() override { std::terminate(); }
        RHIBufferRAII CreateBuffer(
            const RHIBufferCreateDesc& desc,
            StrView
        ) override {
            const auto writableID =
                desc.shaderWrite ? FirstWritableBufferID + bufferCreates.size()
                                 : 0;
            bufferCreates.push_back(desc);

            return std::make_unique<FakeBuffer>(desc.size, writableID);
        }
        RHITextureRAII CreateTexture(
            const RHITextureCreateDesc& desc,
            StrView
        ) override {
            const auto id = FirstTextureID + textureCreates.size();
            textureCreates.push_back(desc);

            return std::make_unique<FakeTexture>(
                desc.format,
                desc.width,
                desc.height,
                id,
                &texturesDestroyed
            );
        }

        RHIGraphicsPipelineStateRAII CreatePipelineState(
            const RHIGraphicsPipelineStateDesc& desc,
            StrView
        ) override {
            if(++creates == failAt)
                throw std::runtime_error("fake compile error");
            pipelineCreates.push_back(desc);

            return std::make_unique<FakeGraphicsPipelineState>(destroyed);
        }
        RHIComputePipelineStateRAII CreatePipelineState(
            const RHIComputePipelineStateDesc& desc,
            StrView
        ) override {
            if(++creates == failAt)
                throw std::runtime_error("fake compile error");
            computeCreates.push_back(desc);

            return std::make_unique<FakeComputePipelineState>(computeDestroyed);
        }

        RHISwapchainRAII CreateSwapchain(
            const RHISwapchainCreateDesc&,
            StrView
        ) override {
            std::terminate();
        }
        RHICommandListRAII CreateCommandList() override { std::terminate(); }

        void Submit(std::span<RHICommandList*>, u64) override {
            std::terminate();
        }
        void SubmitAndPresent(
            std::span<RHICommandList*>,
            RHISwapchain&,
            u64
        ) override {
            std::terminate();
        }
        u64 GetCompletedFrame() const noexcept override {
            return completedFrame;
        }
        void WaitFrame(u64) override { std::terminate(); }
        void WaitIdle() override { std::terminate(); }

        void DeferRetire(std::move_only_function<void()> reclaim) override {
            deferred.push_back(std::move(reclaim));
        }

        // a stride is an alignment here, and strides are no powers of two
        RHIBufferSlice AllocateTransient(u32 size, u32 align) override {
            const auto offset = (transientUsed + align - 1) / align * align;
            CROWY_ASSERT(offset + size <= TransientSize);

            transientUsed = offset + size;
            ++transientAllocations;

            return RHIBufferSlice{
                .buffer = &transient,
                .offset = offset,
                .size = size,
                .cpuPtr = transient.bytes.data() + offset
            };
        }
        RHICapabilities GetCapabilities() const noexcept override {
            return Capabilities;
        }

        void RunDeferred() {
            for(auto& reclaim: deferred)
                reclaim();
            deferred.clear();
        }
    };
}
