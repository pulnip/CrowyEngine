#include <gtest/gtest.h>
#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "Semantics.hpp"
#include "TomlLoader.hpp"

using namespace Crowy;

class CreationTestObject final: public Object{
    CROWY_OBJECT_BODY(CreationTestObject)

public:
    CreationTestObject() = default;
    ~CreationTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(CreationTestObject)
};

CROWY_OBJECT(CreationTestObject)
CROWY_OBJECT_END(CreationTestObject)

TEST(Reflection, CreationTestObject){
    ASSERT_TRUE(IsCreationTestObjectRegistered);

    CStr objectName = "CreationTestObject";
    auto object = ClassRegistry::Create(objectName);
    ASSERT_TRUE(object != nullptr);
    EXPECT_TRUE(object->GetClassName() == objectName);
    EXPECT_TRUE(object->IsA("Object"));
    EXPECT_TRUE(object->IsA(objectName));

    auto testObject = dynamic_cast<CreationTestObject*>(object.get());
    EXPECT_TRUE(testObject != nullptr);
}

class InjectionTestObject final: public Object{
    CROWY_OBJECT_BODY(InjectionTestObject)

public:
    InjectionTestObject() = default;
    ~InjectionTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(InjectionTestObject)

    Transform transform;
};

CROWY_OBJECT(InjectionTestObject)
    .SetProperty("transform", &InjectionTestObject::transform)
    .SetProperty("position", &InjectionTestObject::transform, &Transform::position)
    .SetProperty("rotation", &InjectionTestObject::transform, &Transform::rotation)
    .SetProperty("scale", &InjectionTestObject::transform, &Transform::scale)
CROWY_OBJECT_END(InjectionTestObject)

TEST(Reflection, NestedProperty){
    ASSERT_TRUE(IsInjectionTestObjectRegistered);

    auto object = Crowy::ClassRegistry::Create("InjectionTestObject");
    ASSERT_TRUE(object != nullptr);

    auto nested = dynamic_cast<InjectionTestObject*>(object.get());
    ASSERT_TRUE(nested != nullptr);



    {
        auto dom = Crowy::parseTomlString(R"(
[transform]
position = [1.0, 2.0, 3.0]
rotation = [0.0, 0.0, 0.0, 1.0]
scale = [4.0, 5.0, 6.0]
)");
        Crowy::ApplyProperties(nested, dom);

        Transform expected{
            .position = Vec3{1.0, 2.0, 3.0},
            .rotation = Vec4{0.0, 0.0, 0.0, 1.0},
            .scale = Vec3{4.0, 5.0, 6.0},
        };

        EXPECT_EQ(nested->transform.position, expected.position);
        EXPECT_EQ(nested->transform.rotation, expected.rotation);
        EXPECT_EQ(nested->transform.scale, expected.scale);
    }

    {
        auto dom = Crowy::parseTomlString(R"(
position = [3.0, 2.0, 1.0]
rotation = [1.0, 0.0, 0.0, 0.0]
scale = [6.0, 5.0, 4.0]
)");
        Crowy::ApplyProperties(nested, dom);

        Transform expected{
            .position = Vec3{3.0, 2.0, 1.0},
            .rotation = Vec4{1.0, 0.0, 0.0, 0.0},
            .scale = Vec3{6.0, 5.0, 4.0},
        };

        EXPECT_EQ(nested->transform.position, expected.position);
        EXPECT_EQ(nested->transform.rotation, expected.rotation);
        EXPECT_EQ(nested->transform.scale, expected.scale);
    }
}

struct Health{
    i32 current = 0;
    i32 maximum = 100;
};

struct Stats{
    Health health;
    f32 speed = 1.0f;
};

CROWY_STRUCT(Health)
    .SetProperty("current", &Health::current)
    .SetProperty("maximum", &Health::maximum)
CROWY_STRUCT_END(Health)

CROWY_STRUCT(Stats)
    .SetProperty("health", &Stats::health)
    .SetProperty("speed", &Stats::speed)
CROWY_STRUCT_END(Stats)

class StructTestObject final: public Object{
    CROWY_OBJECT_BODY(StructTestObject)

public:
    StructTestObject() = default;
    ~StructTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(StructTestObject)

    Stats stats;
};

CROWY_OBJECT(StructTestObject)
    .SetProperty("stats", &StructTestObject::stats)
CROWY_OBJECT_END(StructTestObject)

TEST(Reflection, NestedStructDesc){
    ASSERT_TRUE(IsHealthRegistered);
    ASSERT_TRUE(IsStatsRegistered);
    ASSERT_TRUE(IsStructTestObjectRegistered);

    auto object = ClassRegistry::Create("StructTestObject");
    ASSERT_TRUE(object != nullptr);

    auto testObject = dynamic_cast<StructTestObject*>(object.get());
    ASSERT_TRUE(testObject != nullptr);

    // a single registered property, two levels of struct below it
    auto dom = parseTomlString(R"(
[stats]
speed = 3.5

[stats.health]
current = 7
)");
    ApplyProperties(testObject, dom);

    EXPECT_EQ(testObject->stats.speed, 3.5f);
    EXPECT_EQ(testObject->stats.health.current, 7);
    // not specified in the DOM, so the default survives
    EXPECT_EQ(testObject->stats.health.maximum, 100);
}

class AliasTestObject final: public Object{
    CROWY_OBJECT_BODY(AliasTestObject)

public:
    AliasTestObject() = default;
    ~AliasTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(AliasTestObject)

    Stats stats;
};

// a member chain ending on a reflected struct: both features compose
CROWY_OBJECT(AliasTestObject)
    .SetProperty("hp", &AliasTestObject::stats, &Stats::health)
CROWY_OBJECT_END(AliasTestObject)

TEST(Reflection, ChainedStructDesc){
    ASSERT_TRUE(IsAliasTestObjectRegistered);

    auto object = ClassRegistry::Create("AliasTestObject");
    ASSERT_TRUE(object != nullptr);

    auto testObject = dynamic_cast<AliasTestObject*>(object.get());
    ASSERT_TRUE(testObject != nullptr);

    auto dom = parseTomlString(R"(
[hp]
current = 3
maximum = 12
)");
    ApplyProperties(testObject, dom);

    EXPECT_EQ(testObject->stats.health.current, 3);
    EXPECT_EQ(testObject->stats.health.maximum, 12);
    // "stats" itself is not a property of AliasTestObject
    EXPECT_EQ(testObject->stats.speed, 1.0f);
}
