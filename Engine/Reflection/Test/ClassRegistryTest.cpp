#include <gtest/gtest.h>
#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "Semantics.hpp"
#include "JsonLoader.hpp"

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

    Str objectName = "CreationTestObject";
    auto object = ClassRegistry::Create<CreationTestObject>();
    ASSERT_TRUE(object != nullptr);
    EXPECT_TRUE(std::strcmp(object->GetClassName(), "CreationTestObject") == 0);
    EXPECT_TRUE(object->IsA<Object>());
    EXPECT_TRUE(object->IsA<CreationTestObject>());

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
        auto dom = Crowy::parseJsonString(R"({
"transform": {
    "position": [1.0, 2.0, 3.0],
    "rotation": [0.0, 0.0, 0.0, 1.0],
    "scale": [4.0, 5.0, 6.0]
}
})");
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
        auto dom = Crowy::parseJsonString(R"({
"position": [3.0, 2.0, 1.0],
"rotation": [1.0, 0.0, 0.0, 0.0],
"scale": [6.0, 5.0, 4.0]
})");
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
    auto dom = parseJsonString(R"({
"stats": {
    "speed": 3.5,
    "health": {"current": 7}
}
})");
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

    auto dom = parseJsonString(R"({
"hp": {
    "current": 3,
    "maximum": 12
}
})");
    ApplyProperties(testObject, dom);

    EXPECT_EQ(testObject->stats.health.current, 3);
    EXPECT_EQ(testObject->stats.health.maximum, 12);
    // "stats" itself is not a property of AliasTestObject
    EXPECT_EQ(testObject->stats.speed, 1.0f);
}

struct OrderProbe{
    f32 alpha = 0.0f;
    bool beta = false;
    i32 gamma = 0;
    f32 delta = 0.0f;
};

// deliberately not member order
CROWY_STRUCT(OrderProbe)
    .SetProperty("delta", &OrderProbe::delta)
        .SetUIRange(0.0f, 1.0f)
    .SetProperty("alpha", &OrderProbe::alpha)
    .SetProperty("gamma", &OrderProbe::gamma)
    .SetProperty("beta", &OrderProbe::beta)
CROWY_STRUCT_END(OrderProbe)

TEST(Reflection, RegistrationOrderSurvives){
    ASSERT_TRUE(IsOrderProbeRegistered);

    const auto* desc = GetDesc<OrderProbe>();
    ASSERT_EQ(desc->properties.size(), 4u);
    EXPECT_EQ(desc->properties[0].name, "delta");
    EXPECT_EQ(desc->properties[1].name, "alpha");
    EXPECT_EQ(desc->properties[2].name, "gamma");
    EXPECT_EQ(desc->properties[3].name, "beta");
}

TEST(Reflection, RangeReachesTheMeta){
    const auto* desc = GetDesc<OrderProbe>();

    const auto* ranged = desc->Find("delta");
    ASSERT_TRUE(ranged != nullptr);
    ASSERT_TRUE(ranged->meta.uiRange.has_value());
    EXPECT_EQ(ranged->meta.uiRange->first, 0.0f);
    EXPECT_EQ(ranged->meta.uiRange->second, 1.0f);

    const auto* rangeless = desc->Find("alpha");
    ASSERT_TRUE(rangeless != nullptr);
    EXPECT_FALSE(rangeless->meta.uiRange.has_value());
}

TEST(Reflection, FindByName){
    const auto* desc = GetDesc<OrderProbe>();

    const auto* found = desc->Find("gamma");
    ASSERT_TRUE(found != nullptr);
    EXPECT_EQ(found->name, "gamma");

    EXPECT_TRUE(desc->Find("epsilon") == nullptr);
}

class InheritanceBaseObject: public Object{
    CROWY_OBJECT_BODY(InheritanceBaseObject)

public:
    InheritanceBaseObject() = default;
    ~InheritanceBaseObject() = default;
    CROWY_DECLARE_MOVE_ONLY(InheritanceBaseObject)

    f32 baseValue = 0.0f;
};

CROWY_OBJECT(InheritanceBaseObject)
    .SetProperty("baseValue", &InheritanceBaseObject::baseValue)
CROWY_OBJECT_END(InheritanceBaseObject)

class InheritanceChildObject final: public InheritanceBaseObject{
    CROWY_OBJECT_BODY(InheritanceChildObject, InheritanceBaseObject)

public:
    InheritanceChildObject() = default;
    ~InheritanceChildObject() = default;
    CROWY_DECLARE_MOVE_ONLY(InheritanceChildObject)

    f32 ownValue = 0.0f;
};

CROWY_OBJECT(InheritanceChildObject, InheritanceBaseObject)
    .SetProperty("ownValue", &InheritanceChildObject::ownValue)
CROWY_OBJECT_END(InheritanceChildObject)

TEST(Reflection, InheritedPropertyApplies){
    ASSERT_TRUE(IsInheritanceBaseObjectRegistered);
    ASSERT_TRUE(IsInheritanceChildObjectRegistered);

    auto object = ClassRegistry::Create("InheritanceChildObject");
    ASSERT_TRUE(object != nullptr);

    auto child = dynamic_cast<InheritanceChildObject*>(object.get());
    ASSERT_TRUE(child != nullptr);

    auto dom = parseJsonString(R"({
"baseValue": 2.5,
"ownValue": 7.5
})");
    ApplyProperties(child, dom);

    EXPECT_EQ(child->baseValue, 2.5f);
    EXPECT_EQ(child->ownValue, 7.5f);
}

TEST(Reflection, InheritedPropertyStaysOnTheParentTable){
    const auto* desc = GetDesc<InheritanceChildObject>();

    // per-class tables are not merged: a consumer follows parent itself
    EXPECT_TRUE(desc->Find("ownValue") != nullptr);
    EXPECT_TRUE(desc->Find("baseValue") == nullptr);

    ASSERT_TRUE(desc->parent != nullptr);
    EXPECT_TRUE(desc->parent->Find("baseValue") != nullptr);
}

enum class BlendProbe : u8{
    Off = 0,
    Additive = 1,
    // deliberately sparse: value != index
    Multiply = 4,
};

namespace Crowy
{
    CROWY_ENUM_BEGIN(BlendProbe)
        CROWY_ENUM_VALUE(Off)
        CROWY_ENUM_VALUE(Additive)
        CROWY_ENUM_VALUE(Multiply)
    CROWY_ENUM_END()
}

class EnumTestObject final: public Object{
    CROWY_OBJECT_BODY(EnumTestObject)

public:
    EnumTestObject() = default;
    ~EnumTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(EnumTestObject)

    BlendProbe mode = BlendProbe::Off;
};

CROWY_OBJECT(EnumTestObject)
    .SetProperty("mode", &EnumTestObject::mode)
CROWY_OBJECT_END(EnumTestObject)

TEST(Reflection, EnumNameLookupsRoundTrip){
    EXPECT_STREQ(enumName(BlendProbe::Multiply), "Multiply");
    EXPECT_TRUE(enumName(static_cast<BlendProbe>(9)) == nullptr);

    EXPECT_EQ(enumFromName<BlendProbe>("Additive"), BlendProbe::Additive);
    EXPECT_FALSE(enumFromName<BlendProbe>("Screen").has_value());
}

TEST(Reflection, EnumeratorsSurfaceOnTheTypeOps){
    ASSERT_TRUE(IsEnumTestObjectRegistered);

    const auto* prop = GetDesc<EnumTestObject>()->Find("mode");
    ASSERT_TRUE(prop != nullptr);
    EXPECT_STREQ(prop->type.name, "BlendProbe");

    ASSERT_TRUE(prop->type.enumerators != nullptr);
    const auto enumerators = prop->type.enumerators();
    ASSERT_EQ(enumerators.size(), 3u);
    EXPECT_STREQ(enumerators[0].name, "Off");
    EXPECT_EQ(enumerators[1].value, 1);
    EXPECT_STREQ(enumerators[2].name, "Multiply");
    EXPECT_EQ(enumerators[2].value, 4);
}

TEST(Reflection, EnumDeserializesByName){
    auto object = ClassRegistry::Create("EnumTestObject");
    ASSERT_TRUE(object != nullptr);

    auto testObject = dynamic_cast<EnumTestObject*>(object.get());
    ASSERT_TRUE(testObject != nullptr);

    auto dom = parseJsonString(R"({"mode": "Multiply"})");
    ApplyProperties(testObject, dom);
    EXPECT_EQ(testObject->mode, BlendProbe::Multiply);

    // an unknown name keeps the current value, like an absent key
    auto unknown = parseJsonString(R"({"mode": "Screen"})");
    ApplyProperties(testObject, unknown);
    EXPECT_EQ(testObject->mode, BlendProbe::Multiply);
}
