#include <gtest/gtest.h>

#include "RHICommandList.hpp"

using namespace Crowy;

namespace
{
    // the base class holds every pass rule; nothing stands behind it
    class BareCommandList final: public RHICommandList {
    public:
        void BeginEvent(CStr) override {}
        void EndEvent() override {}
        void SetMarker(CStr) override {}
    };
}

TEST(RenderPassRules, ADepthOnlyPassBeginsAndEnds) {
    BareCommandList cmdList;

    cmdList.BeginRenderPass(
        RHIRenderPassDesc{.depthAttachment = RHIDepthAttachment{}}
    );
    cmdList.EndRenderPass();
}

#if defined(_DEBUG) || !defined(NDEBUG)
TEST(RenderPassRulesDeathTest, APassWithNoAttachmentDies) {
    EXPECT_DEATH(
        {
            BareCommandList cmdList;
            cmdList.BeginRenderPass(RHIRenderPassDesc{});
        },
        "a color or a depth attachment"
    );
}
#endif
