#include <cstddef>
#include <type_traits>
#include <vector>

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

// clang-format off
CROWY_OBJECT(CreationTestObject)
CROWY_OBJECT_END(CreationTestObject)
// clang-format on

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

// clang-format off
CROWY_OBJECT(InjectionTestObject)
    .SetProperty("transform", &InjectionTestObject::transform)
    .SetProperty("position", &InjectionTestObject::transform, &Transform::position)
    .SetProperty("rotation", &InjectionTestObject::transform, &Transform::rotation)
    .SetProperty("scale", &InjectionTestObject::transform, &Transform::scale)
CROWY_OBJECT_END(InjectionTestObject)
// clang-format on

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

// clang-format off
CROWY_STRUCT(Health)
    .SetProperty("current", &Health::current)
    .SetProperty("maximum", &Health::maximum)
CROWY_STRUCT_END(Health)

CROWY_STRUCT(Stats)
    .SetProperty("health", &Stats::health)
    .SetProperty("speed", &Stats::speed)
CROWY_STRUCT_END(Stats)
// clang-format on

class StructTestObject final: public Object{
    CROWY_OBJECT_BODY(StructTestObject)

public:
    StructTestObject() = default;
    ~StructTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(StructTestObject)

    Stats stats;
};

// clang-format off
CROWY_OBJECT(StructTestObject)
    .SetProperty("stats", &StructTestObject::stats)
CROWY_OBJECT_END(StructTestObject)
// clang-format on

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
// clang-format off
CROWY_OBJECT(AliasTestObject)
    .SetProperty("hp", &AliasTestObject::stats, &Stats::health)
CROWY_OBJECT_END(AliasTestObject)
// clang-format on

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
// clang-format off
CROWY_STRUCT(OrderProbe)
    .SetProperty("delta", &OrderProbe::delta)
        .SetUIRange(0.0f, 1.0f)
    .SetProperty("alpha", &OrderProbe::alpha)
    .SetProperty("gamma", &OrderProbe::gamma)
    .SetProperty("beta", &OrderProbe::beta)
CROWY_STRUCT_END(OrderProbe)
// clang-format on

TEST(Reflection, RegistrationOrderSurvives){
    ASSERT_TRUE(IsOrderProbeRegistered);

    const auto* desc = GetDesc<OrderProbe>();
    ASSERT_EQ(desc->properties.size(), 4u);
    EXPECT_EQ(desc->properties[0].name, "delta");
    EXPECT_EQ(desc->properties[1].name, "alpha");
    EXPECT_EQ(desc->properties[2].name, "gamma");
    EXPECT_EQ(desc->properties[3].name, "beta");
}

TEST(Reflection, SerializedKeysFollowRegistration) {
    OrderProbe probe;
    DOM::Value value;
    SerializeProperties(&probe, value);

    std::vector<Str> keys;
    for(const auto& [key, _]: *value.asTable()) {
        keys.push_back(key);
    }
    EXPECT_EQ(keys, (std::vector<Str>{"delta", "alpha", "gamma", "beta"}));
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

// clang-format off
CROWY_OBJECT(InheritanceBaseObject)
    .SetProperty("baseValue", &InheritanceBaseObject::baseValue)
CROWY_OBJECT_END(InheritanceBaseObject)
// clang-format on

class InheritanceChildObject final: public InheritanceBaseObject{
    CROWY_OBJECT_BODY(InheritanceChildObject, InheritanceBaseObject)

public:
    InheritanceChildObject() = default;
    ~InheritanceChildObject() = default;
    CROWY_DECLARE_MOVE_ONLY(InheritanceChildObject)

    f32 ownValue = 0.0f;
};

// clang-format off
CROWY_OBJECT(InheritanceChildObject, InheritanceBaseObject)
    .SetProperty("ownValue", &InheritanceChildObject::ownValue)
CROWY_OBJECT_END(InheritanceChildObject)
// clang-format on

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
    // clang-format off
    CROWY_ENUM_BEGIN(BlendProbe)
        CROWY_ENUM_VALUE(Off)
        CROWY_ENUM_VALUE(Additive)
        CROWY_ENUM_VALUE(Multiply)
    CROWY_ENUM_END()
    // clang-format on
}

class EnumTestObject final: public Object{
    CROWY_OBJECT_BODY(EnumTestObject)

public:
    EnumTestObject() = default;
    ~EnumTestObject() = default;
    CROWY_DECLARE_MOVE_ONLY(EnumTestObject)

    BlendProbe mode = BlendProbe::Off;
};

// clang-format off
CROWY_OBJECT(EnumTestObject)
    .SetProperty("mode", &EnumTestObject::mode)
CROWY_OBJECT_END(EnumTestObject)
// clang-format on

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
    ASSERT_TRUE(speed.type != nullptr);
    EXPECT_TRUE(speed.member == &object.stats.speed);
    EXPECT_EQ(speed.type, GetTypeOps<f32>());

    auto current = ResolveProperty(&object, desc, "stats.health.current");
    ASSERT_TRUE(current.type != nullptr);
    EXPECT_TRUE(current.member == &object.stats.health.current);

    // a terminal struct resolves too; it has no leaf serializer
    auto stats = ResolveProperty(&object, desc, "stats");
    ASSERT_TRUE(stats.type != nullptr);
    EXPECT_TRUE(stats.member == &object.stats);
    EXPECT_TRUE(stats.type->serialize == nullptr);
    EXPECT_TRUE(NestedDesc(*stats.type) != nullptr);
}

// a const object resolves to const members only, so a read cannot write
TEST(Reflection, AConstObjectResolvesToAConstMember) {
    const StructTestObject object;
    const auto& desc = *GetDesc<StructTestObject>();

    const auto current = ResolveProperty(&object, desc, "stats.health.current");
    static_assert(std::is_same_v<decltype(current.member), const void*>);
    ASSERT_TRUE(current.type != nullptr);
    EXPECT_TRUE(current.member == &object.stats.health.current);

    const auto* accessor = GetDesc<Health>()->Find("current")->accessor.get();
    static_assert(std::is_same_v<
                  decltype(accessor->Get(&object.stats.health)),
                  const void*>);
    EXPECT_TRUE(accessor->Get(&object.stats.health) == current.member);
}

TEST(Reflection, ResolvePropertyReachesTheParent){
    InheritanceChildObject child;

    auto base = ResolveProperty(&child, *GetDesc<InheritanceChildObject>(), "baseValue");
    ASSERT_TRUE(base.type != nullptr);
    EXPECT_TRUE(base.member == &child.baseValue);
}

TEST(Reflection, ResolvePropertyNamesTheFailure){
    StructTestObject object;
    const auto& desc = *GetDesc<StructTestObject>();

    auto unknown = ResolveProperty(&object, desc, "nope");
    EXPECT_TRUE(unknown.type == nullptr);
    EXPECT_EQ(unknown.error, "no property 'nope' on 'StructTestObject'");

    auto unknownNested = ResolveProperty(&object, desc, "stats.nope");
    EXPECT_TRUE(unknownNested.type == nullptr);
    EXPECT_EQ(unknownNested.error, "no property 'nope' on 'Stats'");

    auto intoLeaf = ResolveProperty(&object, desc, "stats.speed.x");
    EXPECT_TRUE(intoLeaf.type == nullptr);
    EXPECT_EQ(intoLeaf.error, "'speed' has no properties");

    EXPECT_TRUE(ResolveProperty(&object, desc, "").type == nullptr);
    EXPECT_TRUE(ResolveProperty(&object, desc, "stats.").type == nullptr);
    EXPECT_TRUE(ResolveProperty(&object, desc, ".stats").type == nullptr);
    EXPECT_TRUE(ResolveProperty(&object, desc, "stats..speed").type == nullptr);

    auto notAList = ResolveProperty(&object, desc, "stats[0]");
    EXPECT_TRUE(notAList.type == nullptr);
    EXPECT_EQ(notAList.error, "'stats' is not a list");
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

struct ListItem {
    f32 weight = 1.0f;
    Str label;
};

// clang-format off
CROWY_STRUCT(ListItem)
    .SetProperty("weight", &ListItem::weight)
    .SetProperty("label", &ListItem::label)
CROWY_STRUCT_END(ListItem)
// clang-format on

struct ListProbe {
    std::vector<f32> weights;
    std::vector<ListItem> items;
    std::vector<BlendProbe> modes;
};

// clang-format off
CROWY_STRUCT(ListProbe)
    .SetProperty("weights", &ListProbe::weights)
    .SetProperty("items", &ListProbe::items)
    .SetProperty("modes", &ListProbe::modes)
CROWY_STRUCT_END(ListProbe)
// clang-format on

TEST(Reflection, AListIsItsElementsInOrder) {
    const auto& ops = *GetTypeOps<std::vector<ListItem>>();
    ASSERT_TRUE(ops.list != nullptr);
    EXPECT_EQ(ops.list->elementOps(), GetTypeOps<ListItem>());
    EXPECT_TRUE(ops.getDesc == nullptr);

    ListProbe probe{
        .weights = {0.5f, 2.0f},
        .items = {{.weight = 0.25f, .label = "a"}, {}},
        .modes = {BlendProbe::Multiply, BlendProbe::Off}
    };
    DOM::Value value;
    SerializeProperties(&probe, value);
    const auto text = emitJson(value);
    EXPECT_EQ(
        text,
        R"({"weights":[0.5,2.0],"items":[{"weight":0.25,"label":"a"},)"
        R"({"weight":1.0,"label":""}],"modes":["Multiply","Off"]})"
    );

    ListProbe back;
    EXPECT_TRUE(ApplyProperties(&back, parseJsonString(text)).empty());
    EXPECT_EQ(back.weights, probe.weights);
    ASSERT_EQ(back.items.size(), 2u);
    EXPECT_EQ(back.items[0].label, "a");
    EXPECT_EQ(back.modes, probe.modes);
}

TEST(Reflection, AListTakesTheLengthItIsGiven) {
    ListProbe probe{.weights = {1.0f, 2.0f, 3.0f}};
    EXPECT_TRUE(
        ApplyProperties(&probe, parseJsonString(R"({"weights":[9]})")).empty()
    );
    EXPECT_EQ(probe.weights, (std::vector<f32>{9.0f}));

    // a new element starts from its default, and its absent keys keep it
    EXPECT_TRUE(
        ApplyProperties(&probe, parseJsonString(R"({"items":[{"label":"x"}]})"))
            .empty()
    );
    ASSERT_EQ(probe.items.size(), 1u);
    EXPECT_EQ(probe.items[0].weight, 1.0f);
    EXPECT_EQ(probe.items[0].label, "x");
}

TEST(Reflection, AListErrorNamesTheElement) {
    ListProbe probe;
    const auto errors = ApplyProperties(
        &probe,
        parseJsonString(
            R"({"weights":"no","items":[{"weight":"x"},{"nope":1}],)"
            R"("modes":["Bad"]})"
        )
    );
    EXPECT_EQ(
        errors,
        (PropertyErrors{
            "'items[0].weight' expects f32",
            "'modes[0]' expects BlendProbe",
            "'weights' is a list and expects an array",
            "no property 'items[1].nope' on 'ListItem'"
        })
    );
}

TEST(Reflection, AnIndexPicksAnElement) {
    ListProbe probe{
        .weights = {0.5f, 2.0f},
        .items = {{.weight = 0.25f, .label = "a"}, {.label = "b"}}
    };
    const auto& desc = *GetDesc<ListProbe>();

    const auto weight = ResolveProperty(&probe, desc, "weights[1]");
    ASSERT_TRUE(weight.type != nullptr) << weight.error;
    EXPECT_TRUE(weight.member == &probe.weights[1]);
    EXPECT_EQ(weight.type, GetTypeOps<f32>());

    const auto label = ResolveProperty(&probe, desc, "items[1].label");
    ASSERT_TRUE(label.type != nullptr) << label.error;
    EXPECT_TRUE(label.member == &probe.items[1].label);

    const auto item = ResolveProperty(&probe, desc, "items[0]");
    ASSERT_TRUE(item.type != nullptr) << item.error;
    EXPECT_EQ(NestedDesc(*item.type), GetDesc<ListItem>());

    // a list path names the whole list
    const auto list = ResolveProperty(&probe, desc, "items");
    ASSERT_TRUE(list.type != nullptr) << list.error;
    EXPECT_TRUE(list.type->list != nullptr);

    const ListProbe& readOnly = probe;
    const auto constant = ResolveProperty(&readOnly, desc, "weights[0]");
    EXPECT_TRUE(constant.member == &probe.weights[0]);
}

TEST(Reflection, AnIndexNamesItsFailure) {
    ListProbe probe{.weights = {0.5f, 2.0f}};
    const auto& desc = *GetDesc<ListProbe>();

    EXPECT_EQ(
        ResolveProperty(&probe, desc, "weights[2]").error,
        "'weights[2]' is out of range (2 elements)"
    );
    EXPECT_EQ(
        ResolveProperty(&probe, desc, "weights[x]").error,
        "bad index in 'weights[x]'"
    );
    EXPECT_EQ(
        ResolveProperty(&probe, desc, "weights[1]x").error,
        "bad index in 'weights[1]x'"
    );
    EXPECT_EQ(
        ResolveProperty(&probe, desc, "weights[0].x").error,
        "'weights[0]' has no properties"
    );
    EXPECT_EQ(
        ResolveProperty(&probe, desc, "items[0]").error,
        "'items[0]' is out of range (0 elements)"
    );
}
