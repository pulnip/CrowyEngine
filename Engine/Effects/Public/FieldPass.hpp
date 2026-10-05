#pragma once

#include <span>
#include <vector>

#include "ComputeKernel.hpp"
#include "FieldBuffer.hpp"
#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    using FieldBuffers = std::span<FieldBuffer* const>;

    // the most threads one dimension of a dispatch reaches on D3D12: 65535
    // groups of FieldClear's 64
    inline constexpr u32 MaxDispatchThreads = 65535u * 64u;

    // the kernels every field pass shares
    class FieldKernels {
    private:
        ComputeKernel clear;

    public:
        ~FieldKernels();
        CROWY_DECLARE_PINNED(FieldKernels)

        explicit FieldKernels(RHIDevice& device);

        ComputeKernel& Clear() noexcept { return clear; }
    };

    // One compute pass over fields: their acquires, a dispatch barrier where a
    // dispatch touches what one before it touched, and their releases.
    class FieldPass {
    private:
        using Fields = std::vector<FieldBuffer*>;
        using Barriers = std::vector<RHIBufferBarrier>;
        using Flags = std::vector<bool>;

    private:
        RHICommandList& cmdList;
        FieldKernels& kernels;
        Fields fields;
        // per field, touched since its last barrier
        Flags pending;
        Barriers scratch;
        Barriers releases;
        bool open = false;

    public:
        ~FieldPass();
        CROWY_DECLARE_PINNED(FieldPass)

        FieldPass(RHICommandList& cmdList, FieldKernels& kernels);

        // opens the pass; every field is written in it
        void Begin(FieldBuffers fields);
        // zeroes every word of a field of this pass
        void Clear(FieldBuffer& field);
        // `touches`: the fields of this pass the kernel reads or writes
        template<typename Push>
        void Dispatch(
            ComputeKernel& kernel,
            const Push& push,
            Size3D threads,
            FieldBuffers touches
        ) {
            dispatch(kernel, &push, sizeof(Push), threads, touches);
        }
        // closes the pass; the releases are what the readers acquire
        std::span<const RHIBufferBarrier> End();

    private:
        void dispatch(
            ComputeKernel& kernel,
            const void* push,
            u32 size,
            Size3D threads,
            FieldBuffers touches
        );
        usize indexOf(const FieldBuffer& field) const;
    };
}
