#include "UploadRing.hpp"

#include <algorithm>
#include <array>
#include <cstdlib>

#include "Assert.hpp"
#include "IntMath.hpp"
#include "PtrUtil.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIFence.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    void uploadGPUOnlyBuffer(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        RHIBuffer& buffer,
        const RHISubresourceData& sub
    ) {
        const auto totalBytes = buffer.GetSize();

        auto alloc = ring.Allocate(totalBytes, align);

        alloc.buffer.Upload(sub.data, sub.rowPitch, alloc.offset);

        // brand-new resource: first use is a self-contained acquire
        const std::array acquires{makeBarrier(
            buffer,
            RHIResourceUsage::Undefined,
            RHIResourceUsage::CopyDst
        )};
        cmdList.BeginBlitPass({}, acquires);

        cmdList.Copy(alloc.buffer, buffer, alloc.offset, 0, sub.rowPitch);

        // later submissions consume a creation-time upload, so this release
        // stays unmatched until Close, wide open because the readers are
        // unknown
        const std::array releases{RHIBufferBarrier{
            .buffer = &buffer,
            .syncBefore = RHIBarrierSync::Copy,
            .syncAfter = RHIBarrierSync::All,
            .accessBefore = RHIBarrierAccess::CopyDst,
            .accessAfter = RHIBarrierAccess::Common
        }};
        cmdList.EndBlitPass({}, releases);
    }

    void uploadTexture(
        RHICommandList& cmdList,
        UploadRing& ring,
        u64 align,
        Crowy::RHITexture& texture,
        std::span<const RHISubresourceData> subs,
        std::span<const RHISubresourceLayout> layouts,
        const u64 totalBytes
    ) {
        using namespace Crowy;

        const usize n = subs.size();

        auto alloc = ring.Allocate(totalBytes, align);

        for(usize s = 0; s < n; ++s) {
            const auto& layout = layouts[s];
            for(u32 r = 0; r < layout.rowCount; ++r) {
                alloc.buffer.Upload(
                    ptrAdd(subs[s].data, subs[s].rowPitch * r),
                    layout.rowSize,
                    alloc.offset + layout.offset + layout.rowPitch * r
                );
            }
        }

        const auto mipLevels = texture.GetMipLevels();

        // brand-new resource: first use is a self-contained acquire
        const std::array acquires{makeBarrier(
            texture,
            RHIResourceUsage::Undefined,
            RHIResourceUsage::CopyDst
        )};
        cmdList.BeginBlitPass(acquires);

        for(usize s = 0; s < n; ++s) {
            cmdList.Copy(
                alloc.buffer,
                alloc.offset + layouts[s].offset,
                layouts[s].rowPitch,
                texture,
                s % mipLevels,
                s / mipLevels
            );
        }

        // later submissions consume a creation-time upload, so this release
        // stays unmatched until Close, any-shader because the readers are
        // unknown
        const std::array releases{RHITextureBarrier{
            .texture = &texture,
            .syncBefore = RHIBarrierSync::Copy,
            .syncAfter = RHIBarrierSync::AllShading,
            .accessBefore = RHIBarrierAccess::CopyDst,
            .accessAfter = RHIBarrierAccess::ShaderResource,
            .layoutBefore = RHITextureLayout::CopyDst,
            .layoutAfter = RHITextureLayout::ShaderResource
        }};
        cmdList.EndBlitPass(releases);
    }

    UploadRing::UploadRing(
        RHIFence& serialFence,
        RHIBufferRAII stagingBuffer,
        Flush flush
    )
        : fence(&serialFence),
          staging(std::move(stagingBuffer)),
          flush(std::move(flush)),
          capacity(staging->GetSize())
#if CROWY_DEBUG_BUILD
          ,
          poisonMode(std::getenv("CROWY_RHI_POISON") != nullptr)
#endif
    {
        CROWY_ASSERT(nextMul(capacity, 512ull) == capacity);
    }

    UploadRing::Allocation UploadRing::Allocate(u64 size, u64 align) {
        CROWY_ASSERT(
            staging != nullptr,
            "upload ring was never given a staging buffer"
        );
        CROWY_ASSERT(
            size <= capacity,
            "upload ring holds {} bytes, so a {}-byte allocation never fits",
            capacity,
            size
        );

        u64 newHead = head;
        u64 phys = newHead % capacity;

        const u64 alignedPhys = nextMul(phys, align);
        newHead += alignedPhys - phys;
        phys = alignedPhys;

        if(phys + size > capacity) {
            newHead += capacity - phys;
            phys = 0;
        }

        while(newHead + size > tail + capacity) {
            retireCompleted();
            if(newHead + size <= tail + capacity)
                break;

            if(!inFlight.empty()) {
                fence->WaitCPU(inFlight.front().tag);
                continue;
            }

            // a drained ring holds nothing, the end it just skipped included
            if(tail == head) {
                tail = newHead;
                break;
            }

            // nothing is in flight and the ring is full: copies in a command
            // list nobody submitted hold the space, and no serial will ever
            // free it
            const bool canFlush = tail != head && flush != nullptr;
            CROWY_ASSERT(
                canFlush,
                "upload ring cannot free {} bytes - no pending copies to "
                "flush, so raise the staging size instead",
                size
            );
            if(!canFlush)
                break;

            flush();

            CROWY_ASSERT(
                !inFlight.empty(),
                "upload ring flush submitted nothing to wait on"
            );
            if(inFlight.empty())
                break;
        }

        CROWY_ASSERT(phys == newHead % capacity);
        CROWY_ASSERT(phys % align == 0);

        head = newHead + size;
        return Allocation{
            .buffer = *staging,
            .offset = phys,
            .cpuPtr = ptrAdd(staging->GetMappedPtr(), phys)
        };
    }

    void UploadRing::OnSubmit(u64 tag) {
        // an untouched ring, and a batch that allocated nothing, both would
        // otherwise push an entry per submit and grow without bound
        if(capacity == 0 || head == tail)
            return;
        if(!inFlight.empty() && inFlight.back().head == head)
            return;

        inFlight.push_back(InFlight{.head = head, .tag = tag});
    }

    void UploadRing::retireCompleted() {
        const auto completed = fence->GetValue();

        while(!inFlight.empty() && inFlight.front().tag <= completed) {
            const auto retired = inFlight.front().head;

#if CROWY_DEBUG_BUILD
            poisonRange(tail, retired);
#endif

            tail = retired;
            inFlight.pop_front();
        }
    }

#if CROWY_DEBUG_BUILD
    void UploadRing::poisonRange(u64 from, u64 to) {
        if(!poisonMode || from >= to)
            return;

        static constexpr u32 chunkBytes = 4096;
        static const auto pattern = [] {
            std::array<u8, chunkBytes> bytes{};
            bytes.fill(0xCD);
            return bytes;
        }();

        // `from`/`to` are unwrapped, so stepping the physical cursor modulo
        // the capacity splits the range at the seam on its own
        u64 physical = from % capacity;
        u64 remaining = to - from;
        while(remaining > 0) {
            const auto chunk = std::min(
                {remaining,
                 capacity - physical,
                 static_cast<u64>(pattern.size())}
            );
            staging->Upload(
                pattern.data(),
                static_cast<u32>(chunk),
                static_cast<u32>(physical)
            );

            physical = (physical + chunk) % capacity;
            remaining -= chunk;
        }
    }
#endif
}
