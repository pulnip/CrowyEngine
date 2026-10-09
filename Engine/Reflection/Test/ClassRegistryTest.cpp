#include <cstddef>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "JsonLoader.hpp"
#include "Object.hpp"
#include "Semantics.hpp"

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
        EXPECT_TRUE(Crowy::ApplyProperties(nested, dom).empty());

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
        EXPECT_TRUE(Crowy::ApplyProperties(nested, dom).empty());

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
    EXPECT_TRUE(ApplyProperties(testObject, dom).empty());

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
    EXPECT_TRUE(ApplyProperties(testObject, dom).empty());

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
    // one flat table holds the parent's keys and the child's
    EXPECT_TRUE(ApplyProperties(child, dom).empty());

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
    EXPECT_TRUE(ApplyProperties(testObject, dom).empty());
    EXPECT_EQ(testObject->mode, BlendProbe::Multiply);

    // an unknown name is reported and keeps the current value
    auto unknown = parseJsonString(R"({"mode": "Screen"})");
    EXPECT_EQ(
        ApplyProperties(testObject, unknown),
        PropertyErrors{"'mode' expects BlendProbe"}
    );
    EXPECT_EQ(testObject->mode, BlendProbe::Multiply);
}

TEST(Reflection, FindInChainReachesTheParent){
    const auto* desc = GetDesc<InheritanceChildObject>();

    EXPECT_TRUE(desc->FindInChain("ownValue") != nullptr);
    EXPECT_TRUE(desc->FindInChain("baseValue") != nullptr);
    EXPECT_TRUE(desc->FindInChain("nope") == nullptr);
}

TEST(Reflection, SerializedObjectAppliesBack){
    StructTestObject source;
    source.stats.speed = 3.5f;
    source.stats.health.current = 7;
    source.stats.health.maximum = 42;

    DOM::Value out;
    SerializeProperties(&source, out);
    ASSERT_TRUE(out.is_table());
    // two levels of struct below the one registered property
    EXPECT_TRUE(out.at("stats.health.current") != nullptr);

    StructTestObject back;
    EXPECT_TRUE(ApplyProperties(&back, parseJsonString(emitJson(out))).empty());
    EXPECT_EQ(back.stats.speed, 3.5f);
    EXPECT_EQ(back.stats.health.current, 7);
    EXPECT_EQ(back.stats.health.maximum, 42);
}

TEST(Reflection, SerializeFlattensTheParentChain){
    InheritanceChildObject child;
    child.baseValue = 2.5f;
    child.ownValue = 7.5f;

    DOM::Value out;
    SerializeProperties(&child, out);

    // the parent's keys sit beside the child's, the shape ApplyProperties reads
    EXPECT_EQ(out.get<f32>("baseValue"), 2.5f);
    EXPECT_EQ(out.get<f32>("ownValue"), 7.5f);
}

TEST(Reflection, ResolvePropertyWalksDots){
    StructTestObject object;
    const auto& desc = *GetDesc<StructTestObject>();

    auto speed = ResolveProperty(&object, desc, "stats.speed");
    ASSERT_TRUE(speed.desc != nullptr);
    EXPECT_TRUE(speed.member == &object.stats.speed);
    EXPECT_EQ(speed.desc->name, "speed");

    auto current = ResolveProperty(&object, desc, "stats.health.current");
    ASSERT_TRUE(current.desc != nullptr);
    EXPECT_TRUE(current.member == &object.stats.health.current);

    // a terminal struct resolves too; it has no leaf serializer
    auto stats = ResolveProperty(&object, desc, "stats");
    ASSERT_TRUE(stats.desc != nullptr);
    EXPECT_TRUE(stats.member == &object.stats);
    EXPECT_TRUE(stats.desc->type.serialize == nullptr);
    EXPECT_TRUE(NestedDesc(*stats.desc) != nullptr);
}

TEST(Reflection, ResolvePropertyReachesTheParent){
    InheritanceChildObject child;

    auto base = ResolveProperty(&child, *GetDesc<InheritanceChildObject>(), "baseValue");
    ASSERT_TRUE(base.desc != nullptr);
    EXPECT_TRUE(base.member == &child.baseValue);
}

TEST(Reflection, ResolvePropertyNamesTheFailure){
    StructTestObject object;
    const auto& desc = *GetDesc<StructTestObject>();

    auto unknown = ResolveProperty(&object, desc, "nope");
    EXPECT_TRUE(unknown.desc == nullptr);
    EXPECT_EQ(unknown.error, "no property 'nope' on 'StructTestObject'");

    auto unknownNested = ResolveProperty(&object, desc, "stats.nope");
    EXPECT_TRUE(unknownNested.desc == nullptr);
    EXPECT_EQ(unknownNested.error, "no property 'nope' on 'Stats'");

    auto intoLeaf = ResolveProperty(&object, desc, "stats.speed.x");
    EXPECT_TRUE(intoLeaf.desc == nullptr);
    EXPECT_EQ(intoLeaf.error, "'speed' has no properties");

    EXPECT_TRUE(ResolveProperty(&object, desc, "").desc == nullptr);
    EXPECT_TRUE(ResolveProperty(&object, desc, "stats.").desc == nullptr);
    EXPECT_TRUE(ResolveProperty(&object, desc, ".stats").desc == nullptr);
    EXPECT_TRUE(ResolveProperty(&object, desc, "stats..speed").desc == nullptr);

    auto indexed = ResolveProperty(&object, desc, "stats[0]");
    EXPECT_TRUE(indexed.desc == nullptr);
    EXPECT_EQ(indexed.error, "index paths are not supported: 'stats[0]'");
}

TEST(Reflection, ApplyReportsUnknownKeysWithTheirPath) {
    StructTestObject object;

    const auto errors = ApplyProperties(&object, parseJsonString(R"({
"stats": {"speed": 2.0, "sped": 3.0, "health": {"curent": 4}},
"stat": 1
})"));

    // sorted, and what did bind still applied
    EXPECT_EQ(
        errors,
        (PropertyErrors{
            "no property 'stat' on 'StructTestObject'",
            "no property 'stats.health.curent' on 'Health'",
            "no property 'stats.sped' on 'Stats'"
        })
    );
    EXPECT_EQ(object.stats.speed, 2.0f);
}

TEST(Reflection, ApplyReportsAValueThatDoesNotBind) {
    StructTestObject object;

    const auto errors = ApplyProperties(&object, parseJsonString(R"({
"stats": {"speed": "fast", "health": 3}
})"));

    EXPECT_EQ(
        errors,
        (PropertyErrors{
            "'stats.health' is a struct and expects a table",
            "'stats.speed' expects f32"
        })
    );
    EXPECT_EQ(object.stats.speed, 1.0f);
    EXPECT_EQ(object.stats.health.maximum, 100);
}

TEST(Reflection, ApplyReportsARootThatIsNotATable) {
    StructTestObject object;

    EXPECT_EQ(
        ApplyProperties(&object, parseJsonString("[1]")),
        PropertyErrors{"'StructTestObject' expects a table"}
    );
}

TEST(Reflection, ApplyLeavesAbsentKeysAlone) {
    StructTestObject object;

    EXPECT_TRUE(ApplyProperties(&object, parseJsonString("{}")).empty());
    EXPECT_EQ(object.stats.speed, 1.0f);
    EXPECT_EQ(object.stats.health.maximum, 100);
}

TEST(Reflection, FindTypeSeesStructsAndClasses) {
    EXPECT_EQ(ClassRegistry::FindType("Health"), GetDesc<Health>());
    EXPECT_EQ(
        ClassRegistry::FindType("InheritanceChildObject"),
        GetDesc<InheritanceChildObject>()
    );
    EXPECT_TRUE(ClassRegistry::FindType("Nope") == nullptr);

    // a Struct has a name but no factory
    EXPECT_TRUE(ClassRegistry::Create("Health") == nullptr);
}

TEST(Reflection, ANameRegisteredTwiceStopsTheProcess) {
    EXPECT_DEATH(
        ClassRegistry::Get().Register(ClassRegistry::Get().DescFor<Health>()),
        "'Health' is registered twice"
    );

    EXPECT_DEATH(
        {
            StructDesc impostor;
            impostor.name = "Health";
            ClassRegistry::Get().Register(impostor);
        },
        "two types are registered as 'Health'"
    );
}

TEST(Reflection, ATypeFoundByNameLivesInCallerMemory) {
    const auto* desc = ClassRegistry::FindType("Stats");
    ASSERT_TRUE(desc != nullptr);
    ASSERT_TRUE(desc->ops != nullptr);

    const auto& ops = *desc->ops;
    ASSERT_TRUE(ops.construct != nullptr);
    ASSERT_TRUE(ops.moveConstruct != nullptr);
    ASSERT_TRUE(ops.destroy != nullptr);
    ASSERT_LE(ops.align, alignof(std::max_align_t));

    alignas(std::max_align_t) std::byte first[64];
    alignas(std::max_align_t) std::byte second[64];
    ASSERT_LE(ops.size, sizeof(first));

    ops.construct(first);
    // the desc found by name, not a static type, decides what applies
    EXPECT_TRUE(ApplyProperties(*desc, first, parseJsonString(R"({
"speed": 2.5, "health": {"current": 9}
})"))
                    .empty());

    ops.moveConstruct(second, first);
    ops.destroy(first);

    DOM::Value out;
    SerializeProperties(*desc, second, out);
    EXPECT_EQ(out.get<f32>("speed"), 2.5f);
    EXPECT_EQ(out.get<i32>("health.current"), 9);
    EXPECT_EQ(out.get<i32>("health.maximum"), 100);

    ops.destroy(second);
}

TEST(Reflection, ALifetimeTheTypeLacksStaysNull) {
    struct NeedsAnArgument {
        explicit NeedsAnArgument(i32 value)
            : value(value) {}

        i32 value;
    };

    const auto& ops = *GetTypeOps<NeedsAnArgument>();
    EXPECT_TRUE(ops.construct == nullptr);
    EXPECT_TRUE(ops.moveConstruct != nullptr);
    EXPECT_TRUE(ops.destroy != nullptr);
}
