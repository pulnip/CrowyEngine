#include <utility>

#include <gtest/gtest.h>

#include "RHIRetireQueue.hpp"

using namespace Crowy;

namespace
{
    // more than one, so a Defer from inside the teardown has to grow the queue
    constexpr int ResourceCount = 16;

    // defers a reclaim of its own when destroyed, as a D3D12 texture returns
    // its descriptor
    class DefersWhenDestroyed {
    private:
        RHIRetireQueue* queue = nullptr;
        int* reclaimed = nullptr;

    public:
        ~DefersWhenDestroyed() {
            if(queue != nullptr)
                queue->Defer([count = reclaimed] { ++*count; });
        }
        DefersWhenDestroyed(const DefersWhenDestroyed&) = delete;
        DefersWhenDestroyed& operator=(const DefersWhenDestroyed&) = delete;
        DefersWhenDestroyed(DefersWhenDestroyed&& other) noexcept
            : queue(std::exchange(other.queue, nullptr)),
              reclaimed(other.reclaimed) {}
        DefersWhenDestroyed& operator=(DefersWhenDestroyed&&) = delete;

        DefersWhenDestroyed(RHIRetireQueue& queue, int& reclaimed)
            : queue(&queue), reclaimed(&reclaimed) {}
    };
}

TEST(RetireQueue, CollectAllRunsWhatADestroyedReclaimDefers) {
    RHIRetireQueue queue;
    int reclaimed = 0;
    for(int i = 0; i < ResourceCount; ++i)
        queue.Defer([resource = DefersWhenDestroyed(queue, reclaimed)] {});

    queue.CollectAll();

    EXPECT_EQ(reclaimed, ResourceCount);
}
