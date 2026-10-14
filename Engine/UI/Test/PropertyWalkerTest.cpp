#include <vector>

#include <gtest/gtest.h>
#include "ClassRegistry.hpp"
#include "Object.hpp"
#include "PropertyWalker.hpp"

namespace Crowy
{
    struct UIContext{};
}

using namespace Crowy;

struct WalkerNested{
    f32 amount = 0.25f;
};

CROWY_STRUCT(WalkerNested)
    .SetProperty("amount", &WalkerNested::amount)
CROWY_STRUCT_END(WalkerNested)

struct WalkerBase{
    f32 exposure = 1.0f;
};

CROWY_STRUCT(WalkerBase)
    .SetProperty("exposure", &WalkerBase::exposure)
CROWY_STRUCT_END(WalkerBase)

// non-contiguous, and negative in a signed underlying type: the two things
// an index-is-the-value dropdown would get wrong
enum class WalkerBlend: i16{
    Opaque = -1,
    Masked = 7,
    Additive = 300
};

namespace Crowy
{
    CROWY_ENUM_BEGIN(WalkerBlend)
        CROWY_ENUM_VALUE(Opaque)
        CROWY_ENUM_VALUE(Masked)
        CROWY_ENUM_VALUE(Additive)
    CROWY_ENUM_END()
}

struct WalkerFixture: WalkerBase{
    bool visible = true;
    i32 count = 3;
    f32 roughness = 0.5f;
    f32 speed = 2.0f;
    Vec3 tint{1.0f, 0.5f, 0.25f};
    Str title = "untitled";
    u32 flags = 0;
    WalkerNested nested;
    WalkerBlend blend = WalkerBlend::Masked;
};

CROWY_STRUCT(WalkerFixture)
    .Inherits<WalkerBase>()
    .SetProperty("visible", &WalkerFixture::visible)
    .SetProperty("count", &WalkerFixture::count)
    .SetProperty("roughness", &WalkerFixture::roughness)
        .SetUIRange(0.0f, 1.0f)
    .SetProperty("speed", &WalkerFixture::speed)
    .SetProperty("tint", &WalkerFixture::tint)
    .SetProperty("title", &WalkerFixture::title)
    .SetProperty("flags", &WalkerFixture::flags)
    .SetProperty("nested", &WalkerFixture::nested)
    .SetProperty("blend", &WalkerFixture::blend)
CROWY_STRUCT_END(WalkerFixture)

TEST(PropertyWalker, TreeShape){
    ASSERT_TRUE(IsWalkerNestedRegistered);
    ASSERT_TRUE(IsWalkerBaseRegistered);
    ASSERT_TRUE(IsWalkerFixtureRegistered);

    WalkerFixture fixture;
    auto tree = buildPropertyTree(
        "Fixture", &fixture, *GetDesc<WalkerFixture>(), []{}
    );

    auto* root = std::get_if<Collapsing>(&tree);
    ASSERT_TRUE(root != nullptr);
    EXPECT_EQ(root->label, "Fixture");
    EXPECT_TRUE(root->defaultOpen);
    EXPECT_EQ(root->scopeId, &fixture);

    ASSERT_EQ(root->children.size(), 10u);

    // the inherited property comes first, then own declaration order
    auto* exposure = std::get_if<DragFloat>(&root->children[0]);
    ASSERT_TRUE(exposure != nullptr);
    EXPECT_EQ(exposure->label, "exposure");
    EXPECT_EQ(exposure->v, 1.0f);

    auto* visible = std::get_if<Checkbox>(&root->children[1]);
    ASSERT_TRUE(visible != nullptr);
    EXPECT_EQ(visible->label, "visible");
    EXPECT_TRUE(visible->v);

    auto* count = std::get_if<IntField>(&root->children[2]);
    ASSERT_TRUE(count != nullptr);
    EXPECT_EQ(count->label, "count");
    EXPECT_EQ(count->v, 3);

    // a ranged float becomes a slider carrying the range
    auto* roughness = std::get_if<Slider>(&root->children[3]);
    ASSERT_TRUE(roughness != nullptr);
    EXPECT_EQ(roughness->label, "roughness");
    EXPECT_EQ(roughness->v, 0.5f);
    EXPECT_EQ(roughness->v_min, 0.0f);
    EXPECT_EQ(roughness->v_max, 1.0f);

    // a rangeless float becomes a drag
    auto* speed = std::get_if<DragFloat>(&root->children[4]);
    ASSERT_TRUE(speed != nullptr);
    EXPECT_EQ(speed->label, "speed");
    EXPECT_EQ(speed->v, 2.0f);

    auto* tint = std::get_if<Float3Field>(&root->children[5]);
    ASSERT_TRUE(tint != nullptr);
    EXPECT_EQ(tint->label, "tint");
    EXPECT_EQ(tint->v, (Vec3{1.0f, 0.5f, 0.25f}));

    auto* title = std::get_if<SearchBar>(&root->children[6]);
    ASSERT_TRUE(title != nullptr);
    EXPECT_EQ(title->label, "title");
    EXPECT_EQ(title->str, "untitled");

    // a leaf with no widget of its own shows what it holds
    auto* flags = std::get_if<Text>(&root->children[7]);
    ASSERT_TRUE(flags != nullptr);
    EXPECT_EQ(flags->data, "flags: 0");

    auto* nested = std::get_if<Collapsing>(&root->children[8]);
    ASSERT_TRUE(nested != nullptr);
    EXPECT_EQ(nested->label, "nested");
    EXPECT_EQ(nested->scopeId, &fixture.nested);
    ASSERT_EQ(nested->children.size(), 1u);

    auto* amount = std::get_if<DragFloat>(&nested->children[0]);
    ASSERT_TRUE(amount != nullptr);
    EXPECT_EQ(amount->label, "amount");
    EXPECT_EQ(amount->v, 0.25f);

    // a registered enum lists its enumerators in declaration order, and
    // points at the one the member currently holds
    auto* blend = std::get_if<Dropdown>(&root->children[9]);
    ASSERT_TRUE(blend != nullptr);
    EXPECT_EQ(blend->label, "blend");
    EXPECT_EQ(blend->entries, (std::vector<Str>{"Opaque", "Masked", "Additive"}));
    ASSERT_TRUE(blend->current.has_value());
    EXPECT_EQ(*blend->current, 1u);
}

TEST(PropertyWalker, DropdownWritesTheValueNotTheIndex){
    WalkerFixture fixture;
    int dirty = 0;
    UIContext ctx;

    auto tree = buildPropertyTree(
        "Fixture", &fixture, *GetDesc<WalkerFixture>(), [&dirty]{ ++dirty; }
    );
    auto& root = std::get<Collapsing>(tree);
    auto& blend = std::get<Dropdown>(root.children[9]);

    // index 2 is the enumerator 300, not the value 2
    blend.onChanged(ctx, 2);
    EXPECT_EQ(fixture.blend, WalkerBlend::Additive);
    EXPECT_EQ(dirty, 1);

    // and index 0 is negative in a signed underlying type
    blend.onChanged(ctx, 0);
    EXPECT_EQ(fixture.blend, WalkerBlend::Opaque);
    EXPECT_EQ(dirty, 2);
}

TEST(PropertyWalker, DropdownSelectsNothingForAnUnlistedValue){
    WalkerFixture fixture;
    fixture.blend = static_cast<WalkerBlend>(42);

    auto tree = buildPropertyTree(
        "Fixture", &fixture, *GetDesc<WalkerFixture>(), []{}
    );
    auto& root = std::get<Collapsing>(tree);
    auto& blend = std::get<Dropdown>(root.children[9]);

    EXPECT_EQ(blend.entries.size(), 3u);
    EXPECT_FALSE(blend.current.has_value());
}

TEST(PropertyWalker, WritesLandAndNotify){
    WalkerFixture fixture;
    int dirty = 0;
    UIContext ctx;

    auto tree = buildPropertyTree(
        "Fixture", &fixture, *GetDesc<WalkerFixture>(), [&dirty]{ ++dirty; }
    );
    auto& root = std::get<Collapsing>(tree);

    // the inherited leaf writes through to the base member
    std::get<DragFloat>(root.children[0]).onChanged(ctx, 4.0f);
    EXPECT_EQ(fixture.exposure, 4.0f);
    EXPECT_EQ(dirty, 1);

    std::get<Checkbox>(root.children[1]).onChanged(ctx, false);
    EXPECT_FALSE(fixture.visible);

    std::get<IntField>(root.children[2]).onChanged(ctx, 9);
    EXPECT_EQ(fixture.count, 9);

    std::get<Slider>(root.children[3]).onChanged(ctx, 0.75f);
    EXPECT_EQ(fixture.roughness, 0.75f);

    std::get<Float3Field>(root.children[5]).onChanged(ctx, Vec3{0.0f, 1.0f, 0.0f});
    EXPECT_EQ(fixture.tint, (Vec3{0.0f, 1.0f, 0.0f}));

    std::get<SearchBar>(root.children[6]).onChanged(ctx, "renamed");
    EXPECT_EQ(fixture.title, "renamed");

    // the nested write fires the same target's callback
    auto& nested = std::get<Collapsing>(root.children[8]);
    std::get<DragFloat>(nested.children[0]).onChanged(ctx, 0.5f);
    EXPECT_EQ(fixture.nested.amount, 0.5f);

    EXPECT_EQ(dirty, 7);
}

TEST(PropertyWalker, TwoTargetsBindIndependently){
    WalkerFixture a, b;
    int dirtyA = 0, dirtyB = 0;
    UIContext ctx;

    auto treeA = buildPropertyTree(
        "A", &a, *GetDesc<WalkerFixture>(), [&dirtyA]{ ++dirtyA; }
    );
    auto treeB = buildPropertyTree(
        "B", &b, *GetDesc<WalkerFixture>(), [&dirtyB]{ ++dirtyB; }
    );

    auto& rootA = std::get<Collapsing>(treeA);
    auto& rootB = std::get<Collapsing>(treeB);

    // distinct scope IDs are what keep two "roughness" labels
    // from colliding on ImGui's label hash
    EXPECT_NE(rootA.scopeId, rootB.scopeId);

    std::get<Slider>(rootA.children[3]).onChanged(ctx, 0.9f);
    EXPECT_EQ(a.roughness, 0.9f);
    EXPECT_EQ(b.roughness, 0.5f);
    EXPECT_EQ(dirtyA, 1);
    EXPECT_EQ(dirtyB, 0);
}

namespace
{
    // a target that resolves to whichever fixture `current` names, counting
    // the write accesses
    struct SwitchedTarget {
        WalkerFixture* current = nullptr;
        int writes = 0;

        PropertyTarget Target() {
            return PropertyTarget(
                [this]() -> const void* { return current; },
                [this]() -> void* {
                    ++writes;
                    return current;
                },
                *GetDesc<WalkerFixture>()
            );
        }
    };
}

TEST(PropertyWalker, AnEditLandsOnTheObjectTheTargetNamesThen) {
    WalkerFixture first, second;
    SwitchedTarget target{.current = &first};
    int dirty = 0;
    UIContext ctx;

    auto tree =
        buildPropertyTree("Switched", target.Target(), [&dirty] { ++dirty; });
    auto& root = std::get<Collapsing>(tree);

    target.current = &second;
    std::get<Slider>(root.children[3]).onChanged(ctx, 0.75f);
    EXPECT_EQ(second.roughness, 0.75f);
    EXPECT_EQ(first.roughness, 0.5f);

    // the nested member is found from its parent at the moment of the edit
    auto& nested = std::get<Collapsing>(root.children[8]);
    std::get<DragFloat>(nested.children[0]).onChanged(ctx, 0.5f);
    EXPECT_EQ(second.nested.amount, 0.5f);
    EXPECT_EQ(first.nested.amount, 0.25f);
    EXPECT_EQ(dirty, 2);
}

TEST(PropertyWalker, AnEditToAGoneTargetIsDropped) {
    WalkerFixture fixture;
    SwitchedTarget target{.current = &fixture};
    int dirty = 0;
    UIContext ctx;

    auto tree =
        buildPropertyTree("Switched", target.Target(), [&dirty] { ++dirty; });
    auto& root = std::get<Collapsing>(tree);

    target.current = nullptr;
    std::get<Slider>(root.children[3]).onChanged(ctx, 0.75f);
    std::get<Dropdown>(root.children[9]).onChanged(ctx, 0);
    EXPECT_EQ(fixture.roughness, 0.5f);
    EXPECT_EQ(fixture.blend, WalkerBlend::Masked);
    EXPECT_EQ(dirty, 0);
}

TEST(PropertyWalker, ATargetGoneAtBuildShowsThatItIsGone) {
    SwitchedTarget target;

    auto tree = buildPropertyTree("Gone", target.Target(), {});
    auto& root = std::get<Collapsing>(tree);
    ASSERT_EQ(root.children.size(), 1u);
    EXPECT_EQ(std::get<Text>(root.children[0]).data, "gone");
}

// a write access may record a change, so building asks only to read
TEST(PropertyWalker, BuildingReadsAndAnEditWritesOnce) {
    WalkerFixture fixture;
    SwitchedTarget target{.current = &fixture};
    UIContext ctx;

    auto tree = buildPropertyTree("Switched", target.Target(), [] {});
    EXPECT_EQ(target.writes, 0);

    auto& nested = std::get<Collapsing>(std::get<Collapsing>(tree).children[8]);
    std::get<DragFloat>(nested.children[0]).onChanged(ctx, 0.5f);
    EXPECT_EQ(target.writes, 1);
}

TEST(PropertyWalker, ATypeNeverRegisteredStops) {
    struct NeverRegistered {
        f32 value = 0.0f;
    };
    NeverRegistered target;

    EXPECT_DEATH(
        buildPropertyTree(
            "Unregistered",
            &target,
            *GetDesc<NeverRegistered>(),
            {}
        ),
        "a type that was never registered"
    );
}

struct WalkerList {
    std::vector<f32> weights{0.5f, 1.0f};
    std::vector<WalkerNested> parts{WalkerNested{}};
};

// clang-format off: a registration opens a namespace the formatter cannot see
CROWY_STRUCT(WalkerList)
    .SetProperty("weights", &WalkerList::weights)
        .SetUIRange(0.0f, 2.0f)
    .SetProperty("parts", &WalkerList::parts)
CROWY_STRUCT_END(WalkerList)
// clang-format on

TEST(PropertyWalker, AListShowsItsElementsAndALengthControl) {
    WalkerList list;
    auto tree = buildPropertyTree("List", &list, *GetDesc<WalkerList>(), [] {});
    auto& root = std::get<Collapsing>(tree);
    ASSERT_EQ(root.children.size(), 2u);

    auto& weights = std::get<Collapsing>(root.children[0]);
    EXPECT_EQ(weights.label, "weights (2)");
    EXPECT_EQ(weights.scopeId, &list.weights);
    ASSERT_EQ(weights.children.size(), 4u);

    // an element takes the list's range
    auto& second = std::get<Slider>(weights.children[1]);
    EXPECT_EQ(second.label, "[1]");
    EXPECT_EQ(second.v, 1.0f);
    EXPECT_EQ(second.v_max, 2.0f);
    EXPECT_EQ(std::get<TextButton>(weights.children[2]).label, "+");
    EXPECT_EQ(std::get<TextButton>(weights.children[3]).label, "-");

    auto& parts = std::get<Collapsing>(root.children[1]);
    auto& part = std::get<Collapsing>(parts.children[0]);
    EXPECT_EQ(part.label, "[0]");
    EXPECT_EQ(std::get<DragFloat>(part.children[0]).v, 0.25f);
}

TEST(PropertyWalker, AListElementAndItsLengthAreWritten) {
    WalkerList list;
    int dirty = 0;
    UIContext ctx;
    auto tree =
        buildPropertyTree("List", &list, *GetDesc<WalkerList>(), [&dirty] {
            ++dirty;
        });
    auto& root = std::get<Collapsing>(tree);
    auto& weights = std::get<Collapsing>(root.children[0]);

    std::get<Slider>(weights.children[1]).onChanged(ctx, 1.5f);
    EXPECT_EQ(list.weights, (std::vector<f32>{0.5f, 1.5f}));

    auto& parts = std::get<Collapsing>(root.children[1]);
    auto& part = std::get<Collapsing>(parts.children[0]);
    std::get<DragFloat>(part.children[0]).onChanged(ctx, 0.75f);
    EXPECT_EQ(list.parts[0].amount, 0.75f);

    std::get<TextButton>(weights.children[2]).onPressed(ctx);
    EXPECT_EQ(list.weights, (std::vector<f32>{0.5f, 1.5f, 0.0f}));
    EXPECT_EQ(dirty, 3);

    // a shorter list drops the widget's element, and an empty one stays
    std::get<TextButton>(weights.children[3]).onPressed(ctx);
    std::get<TextButton>(weights.children[3]).onPressed(ctx);
    std::get<Slider>(weights.children[1]).onChanged(ctx, 0.25f);
    EXPECT_EQ(list.weights, (std::vector<f32>{0.5f}));

    std::get<TextButton>(weights.children[3]).onPressed(ctx);
    std::get<TextButton>(weights.children[3]).onPressed(ctx);
    EXPECT_TRUE(list.weights.empty());
    EXPECT_EQ(dirty, 6);
}

TEST(PropertyWalker, AListEditToAGoneTargetIsDropped) {
    WalkerList list;
    WalkerList* current = &list;
    int dirty = 0;
    UIContext ctx;
    auto tree = buildPropertyTree(
        "List",
        PropertyTarget(
            [&current]() -> const void* { return current; },
            [&current]() -> void* { return current; },
            *GetDesc<WalkerList>()
        ),
        [&dirty] { ++dirty; }
    );
    auto& root = std::get<Collapsing>(tree);
    auto& weights = std::get<Collapsing>(root.children[0]);
    auto& parts = std::get<Collapsing>(root.children[1]);
    auto& part = std::get<Collapsing>(parts.children[0]);

    current = nullptr;
    std::get<Slider>(weights.children[1]).onChanged(ctx, 1.5f);
    std::get<TextButton>(weights.children[2]).onPressed(ctx);
    std::get<TextButton>(weights.children[3]).onPressed(ctx);
    std::get<DragFloat>(part.children[0]).onChanged(ctx, 0.75f);
    EXPECT_EQ(list.weights, (std::vector<f32>{0.5f, 1.0f}));
    EXPECT_EQ(list.parts[0].amount, 0.25f);
    EXPECT_EQ(dirty, 0);
}
