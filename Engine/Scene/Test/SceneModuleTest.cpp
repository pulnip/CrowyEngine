#include "SceneModuleTest.hpp"
#include "Resource.hpp"
#include "RHIDevice.hpp"

Crowy::RHIDeviceRAII SceneModuleTest::device = nullptr;

void SceneModuleTest::SetUpTestSuite(){
    device = Crowy::createDevice();
}

void SceneModuleTest::SetUp(){
    Crowy::initResourceModule(device.get());
}

void SceneModuleTest::TearDown(){
    Crowy::deinitResourceModule();
}