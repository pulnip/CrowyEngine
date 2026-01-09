#include <gtest/gtest.h>
#include "RHIDevice.hpp"
#include "Resource.hpp"

class ResourceModuleTest: public ::testing::Test{
protected:
    static Crowy::RHIDevicePtr device;
    static constexpr auto testEmbeddedUri = "embedded:cube";
    static constexpr auto testModelUri = "file:asset/Stelle/Stelle.pmx";

    static void SetUpTestSuite(){
        device = Crowy::createDevice();
    }

    void SetUp() override{
        Crowy::initResourceModule(device.get());
    }

    void TearDown() override{
        Crowy::deinitResourceModule();
    }
};

Crowy::RHIDevicePtr ResourceModuleTest::device = nullptr;

TEST_F(ResourceModuleTest, InitState){

}

TEST_F(ResourceModuleTest, LoadEmbedded){
    const auto [mesh, mat] = Crowy::getOrLoad(
        Crowy::ModelRequest{
            .uri = testEmbeddedUri
        }
    );

    EXPECT_TRUE(mesh.isValid());
    EXPECT_TRUE( mat.isValid());
}

TEST_F(ResourceModuleTest, LoadModelFile){
    const auto [mesh, mat] = Crowy::getOrLoad(
        Crowy::ModelRequest{
            .uri = testModelUri
        }
    );

    EXPECT_TRUE(mesh.isValid());
    EXPECT_TRUE(mat.isValid());
}

TEST_F(ResourceModuleTest, RequestWrongUri){
    {
        const auto [mesh, mat] = Crowy::getOrLoad(
            Crowy::ModelRequest{
                .uri = "WrongUri"
            }
        );

        EXPECT_FALSE(mesh.isValid());
        EXPECT_FALSE(mat.isValid());
    }

    {
        const auto [mesh, mat] = Crowy::getOrLoad(
            Crowy::ModelRequest{
                .uri = "WrongScheme:cube"
            }
        );

        EXPECT_FALSE(mesh.isValid());
        EXPECT_FALSE(mat.isValid());
    }
}
