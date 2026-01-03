#include <gtest/gtest.h>
#include "Log.hpp"
#include "ModelImporter.hpp"
#include "ModelSerializer.hpp"

static constexpr auto testModelUri = "file:asset/Stelle/Stelle.pmx";

TEST(ModelSerializer, Serialize){
    Crowy::Logger::instance().setMinLevel(Crowy::LogLevel::Warn);

    auto modelData = *Crowy::importModel(testModelUri);

    auto serialized = Crowy::serializeModel(modelData);
}

TEST(ModelSerializer, SerializeAndDeserialize){
    Crowy::Logger::instance().setMinLevel(Crowy::LogLevel::Warn);

    auto modelData = *Crowy::importModel(testModelUri);

    auto serialized   =  Crowy::serializeModel  ( modelData);
    auto deserialized = *Crowy::deserializeModel(serialized);

    EXPECT_EQ(modelData.axisInfo, deserialized.axisInfo);
}