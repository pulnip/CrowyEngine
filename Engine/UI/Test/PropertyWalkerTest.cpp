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

    auto* flags = std::get_if<Text>(&root->children[7]);
    ASSERT_TRUE(flags != nullptr);
    EXPECT_TRUE(flags->data.starts_with("unsupported:"));

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
