#include <gtest/gtest.h>

#include "ClassRegistry.hpp"

using namespace Crowy;

// nothing here names a symbol of StageInspect.cpp, so only linking
// SampleStage whole keeps its registrations
TEST(StageReflection, TheRowsRegisterWithoutAReference) {
    for(const auto* name: {"StageInstance", "StageQuad", "StageLight"}) {
        const auto* desc = ClassRegistry::FindType(name);
        ASSERT_TRUE(desc != nullptr) << name;
        EXPECT_FALSE(desc->properties.empty()) << name;
    }
}
