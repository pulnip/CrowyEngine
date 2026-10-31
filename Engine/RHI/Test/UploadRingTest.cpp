#include <memory>

#include <gtest/gtest.h>

#include "FakeDevice.hpp"
#include "RHIFence.hpp"
#include "UploadRing.hpp"

using namespace Crowy;

namespace
{
    constexpr u32 RingBytes = 1024;

    class CompletedFence final: public RHIFence {
    public:
        u64 value = 0;

        void WaitCPU(u64, u64) override {}
        u64 GetValue() const override { return value; }
        bool IsComplete(u64 waitValue) const override {
            return waitValue <= value;
        }
    };
}

// a drained ring still has to skip its end, and nothing holds what it skips
TEST(UploadRing, ADrainedRingFitsWhatOnlyFitsPastItsEnd) {
    CompletedFence fence;
    UploadRing ring(fence, std::make_unique<FakeBuffer>(RingBytes));

    (void)ring.Allocate(512, 1);
    ring.OnSubmit(1);
    fence.value = 1;

    EXPECT_EQ(ring.Allocate(600, 1).offset, 0u);
}
