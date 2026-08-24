#include <algorithm>
#include <array>
#include <cstdlib>
#include "Assert.hpp"
#include "IntMath.hpp"
#include "UploadRing.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    UploadRing::UploadRing(
        RHIDevice& device,
        RHIBufferRAII stagingBuffer,
        Flush flush
    )
        : device(&device)
        , staging(std::move(stagingBuffer))
        , flush(std::move(flush))
        , capacity(staging->GetSize())
    #if defined(_DEBUG) || !defined(NDEBUG)
        , poisonMode(std::getenv("CROWY_RHI_POISON") != nullptr)
    #endif
    {
        CROWY_ASSERT(nextMul(capacity, 512ull) == capacity);
    }

    UploadRing::Allocation UploadRing::Allocate(
        u64 size,
        u64 align
    ){
        CROWY_ASSERT(staging != nullptr,
            "upload ring was never given a staging buffer"
        );
        CROWY_ASSERT(size <= capacity,
            "upload ring holds {} bytes, so a {}-byte allocation never fits",
            capacity, size
        );

        u64 newHead = nextMul(head, align);
        if(u64 p = newHead % capacity; p + size > capacity){
            newHead += capacity - p;
        }

        while(newHead + size > tail + capacity){
            retireCompleted();
            if(newHead + size <= tail + capacity)
                break;

            if(!inFlight.empty()){
                device->WaitFrame(inFlight.front().tag);
                continue;
            }

            // Nothing is in flight and the ring is still full: the space is
            // held by copies sitting in a command list nobody submitted, so
            // no frame value will ever free it.
            const bool canFlush = tail != head && flush != nullptr;
            CROWY_ASSERT(canFlush,
                "upload ring cannot free {} bytes - no pending copies to "
                "flush, so raise the staging size instead",
                size
            );
            if(!canFlush)
                break;

            flush();

            CROWY_ASSERT(!inFlight.empty(),
                "upload ring flush submitted nothing to wait on"
            );
            if(inFlight.empty())
                break;
        }

        const u64 phys = newHead % capacity;
        head = newHead + size;
        return Allocation{
            .buffer = *staging,
            .offset = phys
        };
    }

    void UploadRing::OnSubmit(u64 tag){
        // an untouched ring, and a frame that allocated nothing, both would
        // otherwise push an entry per submit and grow without bound
        if(capacity == 0 || head == tail)
            return;
        if(!inFlight.empty() && inFlight.back().head == head)
            return;

        inFlight.push_back(InFlight{
            .head = head,
            .tag = tag
        });
    }

    void UploadRing::retireCompleted(){
        const auto completed = device->GetCompletedFrame();

        while(!inFlight.empty() && inFlight.front().tag <= completed){
            const auto retired = inFlight.front().head;

        #if defined(_DEBUG) || !defined(NDEBUG)
            poisonRange(tail, retired);
        #endif

            tail = retired;
            inFlight.pop_front();
        }
    }

#if defined(_DEBUG) || !defined(NDEBUG)
    void UploadRing::poisonRange(u64 from, u64 to){
        if(!poisonMode || from >= to)
            return;

        static constexpr u32 chunkBytes = 4096;
        static const auto pattern = []{
            std::array<u8, chunkBytes> bytes{};
            bytes.fill(0xCD);
            return bytes;
        }();

        // `from`/`to` are unwrapped, so stepping the physical cursor modulo
        // the capacity splits the range at the seam on its own
        u64 physical = from % capacity;
        u64 remaining = to - from;
        while(remaining > 0){
            const auto chunk = std::min({
                remaining,
                capacity - physical,
                static_cast<u64>(pattern.size())
            });
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
