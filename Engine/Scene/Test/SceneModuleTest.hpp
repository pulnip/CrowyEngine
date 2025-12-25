#pragma once

#include <gtest/gtest.h>
#include "RHIDevice.hpp"

class SceneModuleTest: public ::testing::Test{
protected:
    static Crowy::RHIDevicePtr device;

    static void SetUpTestSuite();

    void SetUp() override;
    void TearDown() override;
};
