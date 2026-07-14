#include "Assert.hpp"
#include "IntMath.hpp"
#include "UploadRing.hpp"
#include "RHIBuffer.hpp"
#include "RHIFence.hpp"

namespace Crowy
{
    UploadRing::UploadRing(
        RHIBufferRAII stagingBuffer
    )
        : staging(std::move(stagingBuffer))
        , capacity(staging->GetSize())
    {
        SMOL_ASSERT(nextMul(capacity, 512ull) == capacity);
    }

    UploadRing::Allocation UploadRing::Allocate(
        u64 size,
        u64 align
    ){
        SMOL_ASSERT(size <= capacity);

        u64 newHead = nextMul(head, align);
        if(u64 p = newHead % capacity; p + size > capacity){
            newHead += capacity - p;
        }

        while(newHead + size > tail + capacity){
            retireOldest(true);
        }

        const u64 phys = newHead % capacity;
        head = newHead + size;
        return Allocation{
            .buffer = *staging,
            .offset = phys
        };
    }

    void UploadRing::OnSubmit(
        RHIFence& fence,
        u64 value
    ){
        inFlight.push_back(InFlight{
            .head = head,
            .fence = fence,
            .value = value
        });
    }

    void UploadRing::retireOldest(bool wait){
        SMOL_ASSERT(!inFlight.empty());
        auto& f = inFlight.front();
        if(f.fence.GetValue() < f.value){
            if(!wait)
                return;

            // blocking
            f.fence.WaitCPU(f.value);
        }
        tail = f.head;
        inFlight.pop_front();
    }
}
