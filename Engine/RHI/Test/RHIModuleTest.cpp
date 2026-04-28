#include <gtest/gtest.h>
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "RHIPipelineState.hpp"

using namespace Crowy;

class RHIModuleTest: public ::testing::Test{
protected:
    static RHIDeviceRAII device;

    static void SetUpTestSuite(){
        device = createDevice();
    }
};

RHIDeviceRAII RHIModuleTest::device = nullptr;

TEST_F(RHIModuleTest, LoadShaderFile){
    // TODO. use binary file instead source file (include problem)
    // auto vs = device->createShader(RHIShaderCreateDesc{
    //     .file = "vs.metal",
    //     .entry = "vertex_main",
    //     .stage = RHIShaderStage::VertexShader,
    //     .debugName = "TestVertexShader"
    // });
    // auto fs = device->createShader(RHIShaderCreateDesc{
    //     .file = "fs.metal",
    //     .entry = "fragment_main",
    //     .stage = RHIShaderStage::FragmentShader,
    //     .debugName = "TestFragmentShader"
    // });

    // ASSERT_TRUE(vs != nullptr);
    // ASSERT_TRUE(fs != nullptr);

    // auto pipeline = device->createGraphicsPipelineState(RHIGraphicsPipelineStateDesc{
    //     .vertexShader = vs.get(),
    //     .pixelShader = fs.get(),
    //     .debugName = "TestPipeline"
    // });

    // EXPECT_TRUE(pipeline != nullptr);
}