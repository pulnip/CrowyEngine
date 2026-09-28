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
