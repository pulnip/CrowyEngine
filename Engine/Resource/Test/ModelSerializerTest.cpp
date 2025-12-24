#include <gtest/gtest.h>
#include "ModelImporter.hpp"
#include "ModelSerializer.hpp"

static constexpr auto testModelUri = "file:asset/Stelle/Stelle.pmx";

TEST(ModelSerializer, Serialize){
    auto modelData = *Crowy::importModel(testModelUri);

    auto serialized = Crowy::serializeModel(modelData);
}

TEST(ModelSerializer, SerializeAndDeserialize){
    auto modelData = *Crowy::importModel(testModelUri);

    auto serialized   =  Crowy::serializeModel  ( modelData);
    auto deserialized = *Crowy::deserializeModel(serialized);

    EXPECT_EQ(modelData.axisInfo, deserialized.axisInfo);
}