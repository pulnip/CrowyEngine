#include <gtest/gtest.h>

#include "RHICommandList.hpp"

using namespace Crowy;

namespace
{
    // the base class holds every pass rule; nothing stands behind it
    class BareCommandList final: public RHICommandList {
    private:
        void beginEvent(CStr) override {}
        void endEvent() override {}
        void setMarker(CStr) override {}
    };
}

TEST(RenderPassRules, ADepthOnlyPassBeginsAndEnds) {
    BareCommandList cmdList;

    cmdList.BeginRenderPass(
        RHIRenderPassDesc{.depthAttachment = RHIDepthAttachment{}}
    );
    cmdList.EndRenderPass();
}

#if CROWY_DEBUG_BUILD
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
