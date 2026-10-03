#include <array>

#include <gtest/gtest.h>

#include "FakeCommandList.hpp"
#include "FakeDevice.hpp"
#include "RHICommandList.hpp"

using namespace Crowy;

namespace
{
    // each test breaks one rule and expects exactly that violation
    class Fixture {
    public:
        FakeCommandList cmdList;
        FakeTexture color{RHIPixelFormat::RGBA8_UNORM, 4, 4, 1};
        FakeTexture depth{RHIPixelFormat::D32_FLOAT, 4, 4, 2};

        std::array<RHIColorAttachment, 1> ColorAttachment(
            RHILoadAction load = RHILoadAction::Clear
        ) {
            return {RHIColorAttachment{.texture = &color, .loadAction = load}};
        }

        RHIDepthAttachment DepthAttachment() {
            return RHIDepthAttachment{.texture = &depth};
        }

        // a depth-only pass with this one acquire and nothing else
        void DepthPass(
            const RHITextureBarrier& acquire,
            std::span<const RHITextureBarrier> releases = {}
        ) {
            const std::array acquires{acquire};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.depthAttachment = DepthAttachment()},
                acquires
            );
            cmdList.EndRenderPass(releases);
        }

        void ExpectOnly(FakeRule rule) const {
            ASSERT_FALSE(cmdList.violations.empty());
            for(const auto& violation: cmdList.violations)
                EXPECT_EQ(violation.rule, rule) << violation.what;
        }
    };
}

// an in-list acquire whose release was dropped: Metal asserts, the fake counts
TEST(FakeCommandList, AnAcquireWithNoReleaseIsUnmatched) {
    Fixture f;
    f.cmdList.Begin();
    f.DepthPass(MakeBarrier(
        f.depth,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::DepthWrite
    ));
    f.cmdList.Close();
    ASSERT_TRUE(f.cmdList.violations.empty());

    const auto edge = MakeBarrier(
        f.depth,
        RHIResourceUsage::DepthWrite,
        RHIResourceUsage::DepthWrite
    );
    f.cmdList.Begin();
    f.DepthPass(edge);
    f.cmdList.Close();
    ASSERT_EQ(f.cmdList.violations.size(), 1u);

    // A releases, B consumes and releases nothing, C acquires the same
    // value: it pairs with A's release, and Metal would order C after A only
    const std::array releases{edge};
    f.cmdList.Begin();
    f.DepthPass(
        MakeBarrier(
            f.depth,
            RHIResourceUsage::Undefined,
            RHIResourceUsage::DepthWrite
        ),
        releases
    );
    f.DepthPass(edge);
    f.DepthPass(edge);
    f.cmdList.Close();

    f.ExpectOnly(FakeRule::UnmatchedAcquire);
    ASSERT_EQ(f.cmdList.violations.size(), 2u);
    EXPECT_EQ(f.cmdList.violations.back().pass, 2u);
}

// a reader left out of the chain: the second release makes the first stale
TEST(FakeCommandList, AReleaseOverAnUnconsumedOneIsStale) {
    Fixture f;
    const auto toRead = MakeBarrier(
        f.color,
        RHIResourceUsage::RenderTarget,
        RHIResourceUsage::SampledFragment
    );
    const std::array releases{toRead};

    f.cmdList.Begin();
    const auto colors = f.ColorAttachment();
    const std::array acquires{MakeBarrier(
        f.color,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::RenderTarget
    )};
    f.cmdList.BeginRenderPass(
        RHIRenderPassDesc{.colorAttachments = colors},
        acquires
    );
    f.cmdList.EndRenderPass(releases);
    f.DepthPass(
        MakeBarrier(
            f.depth,
            RHIResourceUsage::Undefined,
            RHIResourceUsage::DepthWrite
        ),
        releases
    );
    f.cmdList.Close();

    f.ExpectOnly(FakeRule::StaleRelease);
    EXPECT_EQ(f.cmdList.unconsumedAtClose.size(), 1u);
}

// no acquire at all, or one into another usage than the attachment's
TEST(FakeCommandList, AnAttachmentNeedsItsOwnAcquire) {
    Fixture f;
    const auto colors = f.ColorAttachment();

    f.cmdList.Begin();
    f.cmdList.BeginRenderPass(RHIRenderPassDesc{.colorAttachments = colors});
    f.cmdList.EndRenderPass();
    const std::array asRead{MakeBarrier(
        f.color,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::SampledFragment
    )};
    f.cmdList.BeginRenderPass(
        RHIRenderPassDesc{.colorAttachments = colors},
        asRead
    );
    f.cmdList.EndRenderPass();
    f.cmdList.Close();

    f.ExpectOnly(FakeRule::UnacquiredAttachment);
    EXPECT_EQ(f.cmdList.violations.size(), 2u);
}

// a cross-submission acquire that keeps contents names the layout it expects
TEST(FakeCommandList, AnAcquireFromTheWrongLayoutIsCaught) {
    Fixture f;

    f.cmdList.Begin();
    f.DepthPass(MakeCrossSubmissionBarrier(
        f.depth,
        RHIResourceUsage::DepthWrite,
        RHIResourceUsage::DepthWrite
    ));
    f.cmdList.Close();

    f.ExpectOnly(FakeRule::Layout);
}

// Metal starts every encoder with no push and no constant buffers
TEST(FakeCommandList, ADrawNeedsAFreshPushAndView) {
    Fixture f;
    FakeBuffer view{256};
    const auto colors = f.ColorAttachment();
    const std::array acquires{MakeBarrier(
        f.color,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::RenderTarget
    )};
    const u32 push = 7;

    f.cmdList.Begin();
    f.cmdList.BeginRenderPass(
        RHIRenderPassDesc{.colorAttachments = colors},
        acquires
    );
    f.cmdList.SetPushGraphicsConstants(push);
    f.cmdList.SetGraphicsConstantBuffer(view, 0);
    f.cmdList.Draw(3);
    f.cmdList.EndRenderPass();
    ASSERT_TRUE(f.cmdList.violations.empty());

    // the next pass keeps nothing of the last one's bindings
    f.cmdList.BeginRenderPass(
        RHIRenderPassDesc{.colorAttachments = colors},
        acquires
    );
    f.cmdList.Draw(3);
    f.cmdList.EndRenderPass();
    f.cmdList.Close();

    f.ExpectOnly(FakeRule::FreshBindings);
    EXPECT_EQ(f.cmdList.violations.back().pass, 1u);
}

TEST(FakeCommandList, ALoadAfterADiscardingAcquireIsCaught) {
    Fixture f;
    const auto colors = f.ColorAttachment(RHILoadAction::Load);
    const std::array acquires{MakeBarrier(
        f.color,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::RenderTarget
    )};

    f.cmdList.Begin();
    f.cmdList.BeginRenderPass(
        RHIRenderPassDesc{.colorAttachments = colors},
        acquires
    );
    f.cmdList.EndRenderPass();
    f.cmdList.Close();

    f.ExpectOnly(FakeRule::DiscardedLoad);
}

// a compute pass's release feeds a render pass that acquires the same edge;
// what each dispatch ran is recorded
TEST(FakeCommandList, AComputeReleaseFeedsARenderPass) {
    Fixture f;
    FakeBuffer field{64, 0xB100};
    u32 destroyed = 0;
    FakeComputePipelineState kernel{destroyed};
    const std::array fresh{MakeBarrier(
        field,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::StorageCompute
    )};
    const std::array edge{MakeBarrier(
        field,
        RHIResourceUsage::StorageCompute,
        RHIResourceUsage::SampledVertex
    )};
    const std::array acquires{MakeBarrier(
        f.color,
        RHIResourceUsage::Undefined,
        RHIResourceUsage::RenderTarget
    )};
    const std::array<u32, 2> push{7, 9};

    f.cmdList.Begin();
    f.cmdList.BeginComputePass({}, fresh);
    f.cmdList.SetPipelineState(kernel);
    f.cmdList.SetPushComputeConstants(push);
    f.cmdList.Dispatch(Size3D{1000, 1, 1});
    f.cmdList.EndComputePass({}, edge);
    const auto colors = f.ColorAttachment();
    f.cmdList.BeginRenderPass(
        RHIRenderPassDesc{.colorAttachments = colors},
        acquires,
        edge
    );
    f.cmdList.EndRenderPass();
    f.cmdList.Close();

    EXPECT_TRUE(f.cmdList.violations.empty());
    EXPECT_TRUE(f.cmdList.unconsumedBuffersAtClose.empty());
    ASSERT_EQ(f.cmdList.computePasses.size(), 1u);
    const auto& pass = f.cmdList.computePasses[0];
    EXPECT_EQ(pass.after, 0u);
    ASSERT_EQ(pass.dispatches.size(), 1u);
    EXPECT_EQ(pass.dispatches[0].pipeline, &kernel);
    EXPECT_EQ(pass.dispatches[0].threads, (Size3D{1000, 1, 1}));
    EXPECT_EQ(pass.dispatches[0].push.size(), sizeof(push));
    EXPECT_EQ(
        pass.log,
        (FakeCommandList::Log{"pipeline", "push", "dispatch"})
    );
    EXPECT_EQ(pass.bufferReleases.size(), 1u);
}

// compute halves pair as render halves do: an in-list acquire with no
// release is unmatched, a second unconsumed release is stale
TEST(FakeCommandList, ComputeHalvesPairLikeRenderHalves) {
    Fixture f;
    FakeBuffer field{64, 0xB100};
    const std::array again{MakeBarrier(
        field,
        RHIResourceUsage::SampledVertex,
        RHIResourceUsage::StorageCompute
    )};
    f.cmdList.Begin();
    f.cmdList.BeginComputePass({}, again);
    f.cmdList.EndComputePass();
    f.cmdList.Close();
    f.ExpectOnly(FakeRule::UnmatchedAcquire);

    Fixture g;
    const std::array edge{MakeBarrier(
        field,
        RHIResourceUsage::StorageCompute,
        RHIResourceUsage::SampledVertex
    )};
    g.cmdList.Begin();
    g.cmdList.BeginComputePass();
    g.cmdList.EndComputePass({}, edge);
    g.cmdList.BeginComputePass();
    g.cmdList.EndComputePass({}, edge);
    g.cmdList.Close();
    g.ExpectOnly(FakeRule::StaleRelease);
}

// a UAV hazard between two dispatches of one pass is recorded in place
TEST(FakeCommandList, ADispatchBarrierIsRecordedInItsPass) {
    Fixture f;
    FakeBuffer field{64, 0xB100};
    const std::array hazard{MakeBarrier(
        field,
        RHIResourceUsage::StorageCompute,
        RHIResourceUsage::StorageCompute
    )};

    f.cmdList.Begin();
    f.cmdList.BeginComputePass();
    f.cmdList.Dispatch(Size3D{16, 1, 1});
    f.cmdList.DispatchBarrier({}, hazard);
    f.cmdList.Dispatch(Size3D{16, 1, 1});
    f.cmdList.EndComputePass();
    f.cmdList.Close();

    EXPECT_TRUE(f.cmdList.violations.empty());
    const auto& pass = f.cmdList.computePasses.at(0);
    ASSERT_EQ(pass.dispatchBarriers.size(), 1u);
    EXPECT_EQ(pass.dispatchBarriers[0].size(), 1u);
    EXPECT_EQ(
        pass.log,
        (FakeCommandList::Log{"dispatch", "barrier", "dispatch"})
    );
}

// compute pipelines are counted, and shader-writable buffers answer
// writable ids of their own
TEST(FakeDevice, ComputePipelinesAndWritableBuffers) {
    FakeDevice device;
    const RHIComputePipelineStateDesc desc{
        .computeShader = {.path = "X.slang", .entryPoint = "cs_main"}
    };
    {
        const auto pipeline = device.CreatePipelineState(desc, "kernel");
        ASSERT_EQ(device.computeCreates.size(), 1u);
        EXPECT_EQ(device.computeCreates[0].computeShader.entryPoint, "cs_main");
    }
    EXPECT_EQ(device.computeDestroyed, 1u);

    const auto plain = device.CreateBuffer({.size = 16}, "plain");
    const auto first = device.CreateBuffer(
        {.size = 16, .shaderWrite = true},
        "first"
    );
    const auto second = device.CreateBuffer(
        {.size = 16, .shaderWrite = true},
        "second"
    );
    EXPECT_EQ(
        first->GetWritableID(4u),
        FakeDevice::FirstWritableBufferID + 1
    );
    EXPECT_EQ(
        second->GetWritableID(4u),
        FakeDevice::FirstWritableBufferID + 2
    );
}

