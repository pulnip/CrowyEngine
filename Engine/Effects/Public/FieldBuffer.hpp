#pragma once

#include "Primitives.hpp"
#include "RHICommandList.hpp"
#include "RHIDefinitions.hpp"
#include "RHIFWD.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    // A GPU buffer kept across frames that kernels write and draws read; it
    // remembers where the last submission left it, for the next acquire.
    class FieldBuffer {
    private:
        RHIBufferRAII buffer;
        u32 count = 0;
        u32 stride = 0;
        // what reads it after a writing pass: the release's usage
        RHIResourceUsage reader = RHIResourceUsage::SampledVertex;
        // where the last writing pass left it; Undefined while fresh
        RHIResourceUsage resting = RHIResourceUsage::Undefined;

    public:
        ~FieldBuffer();
        CROWY_DECLARE_PINNED(FieldBuffer)

        // throws std::invalid_argument for no elements, a stride that is not
        // whole 32-bit words, or 4 GiB or more
        FieldBuffer(
            RHIDevice& device,
            u32 count,
            u32 stride,
            StrView name,
            RHIResourceUsage reader = RHIResourceUsage::SampledVertex
        );

        // nothing has written it, so its contents are undefined: clear it or
        // write every element first
        bool Fresh() const noexcept {
            return resting == RHIResourceUsage::Undefined;
        }
        // the first write of a frame
        RHIBufferBarrier AcquireForWrite() const;
        // the end of the writing pass, into the reader
        [[nodiscard]] RHIBufferBarrier Release();

        u64 Writable();
        u64 Readable();
        RHIBuffer& Buffer() noexcept { return *buffer; }
        u32 Count() const noexcept { return count; }
        u32 Stride() const noexcept { return stride; }
        u32 Bytes() const noexcept { return count * stride; }
        RHIResourceUsage Reader() const noexcept { return reader; }
    };
}
