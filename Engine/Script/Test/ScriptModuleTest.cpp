#include <cstddef>
#include <gtest/gtest.h>
#include "EntityHandle.hpp"
#include "Script.hpp"
#include "ScriptSpec.hpp"

class ScriptModuleTest: public ::testing::Test{
protected:
    static void SetUpTestSuite(){
        Crowy::initScriptModule();
    }

    void TearDown() override{
        Crowy::unloadScriptConfig();
    }

    static void TearDownTestSuite(){
        Crowy::deinitScriptModule();
    }
};

TEST_F(ScriptModuleTest, CallSingleScript){
    auto moduleSpec = Crowy::ScriptSpec{
        .modules = {
            Crowy::ScriptModuleSpec{
                .name = "TestModule",
                .files = {
                    "asset/Scripts/TestComponents.as"
                }
            }
        }
    };
    auto instanceSpec = Crowy::ScriptInstanceSpec{
        .monoScripts = {
            "TestComponent"
        }
    };

    ASSERT_NO_THROW(
        Crowy::loadScriptConfig(moduleSpec);
    );

    auto handle = Crowy::ScriptHandle::invalidHandle();
    ASSERT_NO_THROW(
        handle = Crowy::createScriptInstance(instanceSpec, {
            .ptr = nullptr,
            .bit = 0
        });
    );

    EXPECT_NO_THROW({
        Crowy::startScript(handle);
        Crowy::updateScript(handle, 1.0f/60);
        Crowy::finishScript(handle);
    });
}
