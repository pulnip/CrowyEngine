#include <array>
#include <cstring>
#include <span>
#include <stdexcept>
#include <vector>

#include <gtest/gtest.h>

#include "ComputeKernel.hpp"
#include "EffectRandom.hpp"
#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "FieldBuffer.hpp"
#include "FieldPass.hpp"
#include "RHICommandList.hpp"

using namespace Crowy;

namespace
{
    using Log = FakeCommandList::Log;

    // a render pass whose begin acquires `releases`, as a hook pass does
    void readInRenderPass(
        FakeCommandList& cmdList,
        FakeTexture& color,
        std::span<const RHIBufferBarrier> releases
    ) {
        const std::array colors{RHIColorAttachment{.texture = &color}};
        const std::array acquires{MakeBarrier(
            color,
            RHIResourceUsage::Undefined,
            RHIResourceUsage::RenderTarget
        )};
        cmdList.BeginRenderPass(
            RHIRenderPassDesc{.colorAttachments = colors},
            acquires,
            releases
        );
        cmdList.EndRenderPass();
    }

    // a kernel's push as the effects lay it out: the field, then parameters
    struct StepPush {
        u64 field = 0;
        u32 count = 0;
        u32 step = 0;
    };

    class Fixture {
    public:
        FakeDevice device;
        FakeCommandList cmdList;
        FakeTexture color{RHIPixelFormat::RGBA16_FLOAT, 4, 4, 0xC0};
        FieldKernels kernels{device};
        ComputeKernel step{
            device,
            RHIShaderDesc{.path = "Engine/X.slang", .entryPoint = "cs_step"}
        };
    };
}

TEST(FieldBuffer, IsAShaderWritableGPUBufferWithoutInitialData) {
    FakeDevice device;
    FieldBuffer field(device, 512, 32, "embers");

    ASSERT_EQ(device.bufferCreates.size(), 1u);
    const auto& desc = device.bufferCreates[0];
    EXPECT_EQ(desc.size, 16384u);
    EXPECT_EQ(desc.memory, RHIMemoryType::GPUOnly);
    EXPECT_TRUE(desc.shaderWrite);
    EXPECT_EQ(desc.initialData, nullptr);
    EXPECT_TRUE(field.Fresh());
    EXPECT_EQ(field.Count(), 512u);
    EXPECT_EQ(field.Stride(), 32u);
    EXPECT_EQ(field.Bytes(), 16384u);
    EXPECT_EQ(field.Writable(), FakeDevice::FirstWritableBufferID);
}

TEST(FieldBuffer, RefusesEmptyUnalignedOrHugeFields) {
    FakeDevice device;
    EXPECT_THROW(FieldBuffer(device, 0, 16, "none"), std::invalid_argument);
    EXPECT_THROW(FieldBuffer(device, 8, 6, "odd"), std::invalid_argument);
    EXPECT_THROW(FieldBuffer(device, 8, 0, "flat"), std::invalid_argument);
    EXPECT_THROW(
        FieldBuffer(device, 1u << 30, 16, "huge"),
        std::invalid_argument
    );
    EXPECT_TRUE(device.bufferCreates.empty());
}

// the first write acquires from Undefined; after a release, every write
// acquires across submissions from the reader
TEST(FieldBuffer, AcquiresFromUndefinedOnceThenAcrossSubmissions) {
    FakeDevice device;
    FieldBuffer field(device, 64, 16, "field");
    FieldBuffer copied(device, 64, 16, "copied", RHIResourceUsage::CopySrc);

    EXPECT_EQ(
        field.AcquireForWrite(),
        MakeBarrier(
            field.Buffer(),
            RHIResourceUsage::Undefined,
            RHIResourceUsage::StorageCompute
        )
    );
    EXPECT_EQ(
        field.Release(),
        MakeBarrier(
            field.Buffer(),
            RHIResourceUsage::StorageCompute,
            RHIResourceUsage::SampledVertex
        )
    );
    EXPECT_FALSE(field.Fresh());
    const auto again = field.AcquireForWrite();
    EXPECT_TRUE(again.crossSubmission);
    EXPECT_EQ(
        again,
        MakeCrossSubmissionBarrier(
            field.Buffer(),
            RHIResourceUsage::SampledVertex,
            RHIResourceUsage::StorageCompute
        )
    );

    EXPECT_EQ(
        copied.Release(),
        MakeBarrier(
            copied.Buffer(),
            RHIResourceUsage::StorageCompute,
            RHIResourceUsage::CopySrc
        )
    );
}

// a fresh field: acquired from Undefined, cleared word by word, stepped
// behind a barrier, released into the pass that draws it
TEST(FieldPass, AFreshFieldIsClearedThenSteppedThenReleased) {
    Fixture f;
    FieldBuffer field(f.device, 512, 32, "embers");
    FieldBuffer* const fields[] = {&field};
    FieldPass pass(f.cmdList, f.kernels);

    f.cmdList.Begin();
    pass.Begin(fields);
    pass.Clear(field);
    const StepPush push{.field = field.Writable(), .count = 512, .step = 3};
    pass.Dispatch(f.step, push, Size3D{512, 1, 1}, fields);
    const auto releases = pass.End();
    readInRenderPass(f.cmdList, f.color, releases);
    f.cmdList.Close();

    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_TRUE(f.cmdList.unconsumedBuffersAtClose.empty());
    ASSERT_EQ(f.cmdList.computePasses.size(), 1u);
    const auto& recorded = f.cmdList.computePasses[0];
    ASSERT_EQ(recorded.bufferAcquires.size(), 1u);
    EXPECT_FALSE(recorded.bufferAcquires[0].crossSubmission);
    EXPECT_EQ(
        recorded.log,
        (Log{
            "pipeline",
            "push",
            "dispatch",
            "barrier",
            "pipeline",
            "push",
            "dispatch"
        })
    );

    ASSERT_EQ(recorded.dispatches.size(), 2u);
    const auto& clear = recorded.dispatches[0];
    EXPECT_EQ(clear.pipeline, &f.kernels.Clear().Pipeline());
    EXPECT_EQ(clear.threads, (Size3D{4096, 1, 1}));
    ASSERT_EQ(clear.push.size(), 16u);
    u64 words = 0;
    u32 count = 0;
    u32 stride = 0;
    std::memcpy(&words, clear.push.data(), sizeof(words));
    std::memcpy(&count, clear.push.data() + 8, sizeof(count));
    std::memcpy(&stride, clear.push.data() + 12, sizeof(stride));
    EXPECT_EQ(words, FakeDevice::FirstWritableBufferID);
    EXPECT_EQ(count, 4096u);
    EXPECT_EQ(stride, 4096u);

    const auto& stepped = recorded.dispatches[1];
    EXPECT_EQ(stepped.pipeline, &f.step.Pipeline());
    EXPECT_EQ(stepped.threads, (Size3D{512, 1, 1}));
    ASSERT_EQ(stepped.push.size(), sizeof(StepPush));
    EXPECT_EQ(std::memcmp(stepped.push.data(), &push, sizeof(push)), 0);

    ASSERT_EQ(recorded.dispatchBarriers.size(), 1u);
    EXPECT_EQ(recorded.dispatchBarriers[0].size(), 1u);
    ASSERT_EQ(releases.size(), 1u);
    EXPECT_EQ(recorded.bufferReleases[0], releases[0]);
}

// a barrier only where a dispatch touches a field an earlier one touched
// since that field's last barrier
TEST(FieldPass, BarriersFollowWhatEachDispatchTouches) {
    Fixture f;
    FieldBuffer a(f.device, 64, 16, "a");
    FieldBuffer b(f.device, 64, 16, "b");
    FieldBuffer* const both[] = {&a, &b};
    FieldBuffer* const onlyA[] = {&a};
    FieldBuffer* const onlyB[] = {&b};
    FieldPass pass(f.cmdList, f.kernels);
    const StepPush push{};

    f.cmdList.Begin();
    pass.Begin(both);
    pass.Dispatch(f.step, push, Size3D{64, 1, 1}, onlyA);
    pass.Dispatch(f.step, push, Size3D{64, 1, 1}, onlyB);
    pass.Dispatch(f.step, push, Size3D{64, 1, 1}, onlyA);
    pass.Dispatch(f.step, push, Size3D{64, 1, 1}, both);
    const auto releases = pass.End();
    readInRenderPass(f.cmdList, f.color, releases);
    f.cmdList.Close();

    EXPECT_TRUE(f.cmdList.violations.empty());
    const auto& recorded = f.cmdList.computePasses.at(0);
    EXPECT_EQ(recorded.dispatches.size(), 4u);
    ASSERT_EQ(recorded.dispatchBarriers.size(), 2u);
    ASSERT_EQ(recorded.dispatchBarriers[0].size(), 1u);
    EXPECT_EQ(recorded.dispatchBarriers[0][0].buffer, &a.Buffer());
    EXPECT_EQ(recorded.dispatchBarriers[1].size(), 2u);
    EXPECT_EQ(releases.size(), 2u);
}

// the second frame's pass acquires what the first released, across the
// submission
TEST(FieldPass, TheSecondFrameAcquiresAcrossSubmissions) {
    Fixture f;
    FieldBuffer field(f.device, 64, 16, "field");
    FieldBuffer* const fields[] = {&field};
    FieldPass pass(f.cmdList, f.kernels);
    const StepPush push{};

    for(u32 frame = 0; frame < 2; ++frame) {
        f.cmdList.Begin();
        pass.Begin(fields);
        if(frame == 0)
            pass.Clear(field);
        pass.Dispatch(f.step, push, Size3D{64, 1, 1}, fields);
        readInRenderPass(f.cmdList, f.color, pass.End());
        f.cmdList.Close();
    }

    EXPECT_TRUE(f.cmdList.violations.empty());
    const auto& second = f.cmdList.computePasses.at(0);
    ASSERT_EQ(second.bufferAcquires.size(), 1u);
    EXPECT_TRUE(second.bufferAcquires[0].crossSubmission);
    EXPECT_EQ(second.dispatches.size(), 1u);
    EXPECT_TRUE(second.dispatchBarriers.empty());
}

// the CPU twin of Random.slang against the reference permutation
TEST(EffectRandom, MatchesThePCGReference) {
    EXPECT_EQ(pcgHash(1), 0xa8beea3cu);
    EXPECT_EQ(pcgHash(0xffffffffu), 0xe62a4902u);
    EXPECT_EQ(effectHash(1, 1, 0, 0), 0x8f98bf5eu);
    EXPECT_EQ(effectHash(1, 4095, 0, 0), 0x0a8f7de9u);
    EXPECT_EQ(
        effectRandom(1, 0, 0, 0),
        static_cast<f32>(0x01443b47u >> 8) / 16777216.0f
    );
}

// a field past one dimension's dispatch limit is cleared by fewer threads,
// each striding over several words
TEST(FieldPass, AClearStaysInsideTheDispatchLimit) {
    Fixture f;
    FieldBuffer field(f.device, 262144, 64, "wide");
    FieldBuffer* const fields[] = {&field};
    FieldPass pass(f.cmdList, f.kernels);

    f.cmdList.Begin();
    pass.Begin(fields);
    pass.Clear(field);
    readInRenderPass(f.cmdList, f.color, pass.End());
    f.cmdList.Close();

    const auto& clear = f.cmdList.computePasses.at(0).dispatches.at(0);
    EXPECT_EQ(clear.threads, (Size3D{MaxDispatchThreads, 1, 1}));
    u32 count = 0;
    u32 stride = 0;
    std::memcpy(&count, clear.push.data() + 8, sizeof(count));
    std::memcpy(&stride, clear.push.data() + 12, sizeof(stride));
    EXPECT_EQ(count, 262144u * 16u);
    EXPECT_EQ(stride, MaxDispatchThreads);
}

