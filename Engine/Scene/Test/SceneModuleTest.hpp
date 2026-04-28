#pragma once

#include <gtest/gtest.h>
#include "RHIFWD.hpp"

class SceneModuleTest: public ::testing::Test{
protected:
    static Crowy::RHIDeviceRAII device;

    static void SetUpTestSuite();

    void SetUp() override;
    void TearDown() override;
};
