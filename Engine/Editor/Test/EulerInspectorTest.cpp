#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "EulerInspector.hpp"
#include "LinearAlgebra.hpp"

namespace Crowy
{
    struct UIContext {};
}

using namespace Crowy;

struct EulerTurned {
    Quat rotation = unitQuat();
    Vec4 tint = Vec4{0.1f, 0.2f, 0.3f, 0.4f};
};

// clang-format off
CROWY_STRUCT(EulerTurned)
    .AddProperty("rotation", &EulerTurned::rotation)
    .AddProperty("tint", &EulerTurned::tint)
CROWY_STRUCT_END(EulerTurned)
// clang-format on

namespace
{
    Widget buildTurned(
        EulerInspector& inspector,
        EulerTurned& turned,
        CStr label = "Turned"
    ) {
        return buildPropertyTree(
            label,
            &turned,
            *getDesc<EulerTurned>(),
            [] {},
            inspector.Overrides()
        );
    }

    Float3Field& rotationOf(Widget& tree) {
        return std::get<Float3Field>(std::get<Collapsing>(tree).children[0]);
    }
}

TEST(EulerInspector, AQuatShowsAsEulerDegreesAndStoresAQuaternion) {
    ASSERT_TRUE(IsEulerTurnedRegistered);
    EulerInspector inspector;
    EulerTurned turned;
    turned.rotation = rotateY(radiansOf(90.0f));
    UIContext ctx;

    auto tree = buildTurned(inspector, turned);
    EXPECT_NEAR(rotationOf(tree).v.x, 0.0f, 1e-3f);
    EXPECT_NEAR(rotationOf(tree).v.y, 90.0f, 1e-3f);
    EXPECT_NEAR(rotationOf(tree).v.z, 0.0f, 1e-3f);

    rotationOf(tree).onChanged(ctx, Vec3{30.0f, 0.0f, 0.0f});
    EXPECT_EQ(turned.rotation, Quat(rotateX(radiansOf(30.0f))));
}

TEST(EulerInspector, TypedDegreesSurviveARebuild) {
    EulerInspector inspector;
    EulerTurned turned;
    UIContext ctx;

    auto tree = buildTurned(inspector, turned);
    rotationOf(tree).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});

    auto rebuilt = buildTurned(inspector, turned);
    EXPECT_EQ(rotationOf(rebuilt).v, (Vec3{0.0f, 370.0f, 0.0f}));
}

TEST(EulerInspector, TypedDegreesComeBackAfterAnotherSelection) {
    EulerInspector inspector;
    EulerTurned first, second;
    second.rotation = rotateX(radiansOf(20.0f));
    EulerTurned* current = &first;
    UIContext ctx;
    const auto build = [&] {
        return buildPropertyTree(
            "transform",
            PropertyTarget(
                [&current]() -> const void* { return current; },
                [&current]() -> void* { return current; },
                *getDesc<EulerTurned>()
            ),
            [] {},
            inspector.Overrides()
        );
    };

    auto tree = build();
    rotationOf(tree).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});

    current = &second;
    auto other = build();
    EXPECT_NEAR(rotationOf(other).v.x, 20.0f, 1e-3f);
    rotationOf(other).onChanged(ctx, Vec3{40.0f, 0.0f, 0.0f});

    current = &first;
    auto back = build();
    EXPECT_EQ(rotationOf(back).v, (Vec3{0.0f, 370.0f, 0.0f}));
}

TEST(EulerInspector, AnOutsideWriteDecomposesTheRotationAgain) {
    EulerInspector inspector;
    EulerTurned turned;
    UIContext ctx;

    auto tree = buildTurned(inspector, turned);
    rotationOf(tree).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});
    turned.rotation = rotateY(radiansOf(90.0f));

    auto rebuilt = buildTurned(inspector, turned);
    EXPECT_NEAR(rotationOf(rebuilt).v.y, 90.0f, 1e-3f);
}

TEST(EulerInspector, BuildingRemembersNothing) {
    EulerInspector inspector;
    EulerTurned turned;
    turned.rotation = rotateY(radiansOf(370.0f));

    auto first = buildTurned(inspector, turned);
    const auto decomposed = rotationOf(first).v;
    UIContext ctx;
    rotationOf(first).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});
    turned.rotation = rotateX(radiansOf(10.0f));
    auto second = buildTurned(inspector, turned);
    turned.rotation = rotateY(radiansOf(370.0f));
    auto third = buildTurned(inspector, turned);

    EXPECT_NEAR(decomposed.y, 10.0f, 1e-3f);
    EXPECT_EQ(rotationOf(third).v, (Vec3{0.0f, 370.0f, 0.0f}));
}

TEST(EulerInspector, ARotationInTwoSectionsShowsWhatWasTypedInEither) {
    EulerInspector inspector;
    EulerTurned turned;
    UIContext ctx;

    auto section = buildTurned(inspector, turned, "camera transform");
    rotationOf(section).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});

    auto selected = buildTurned(inspector, turned, "transform");
    EXPECT_EQ(rotationOf(selected).v, (Vec3{0.0f, 370.0f, 0.0f}));
}

TEST(EulerInspector, EachSectionKeepsItsOwnTypedDegrees) {
    EulerInspector inspector;
    EulerTurned a, b;
    UIContext ctx;

    auto treeA = buildTurned(inspector, a, "lamp transform");
    auto treeB = buildTurned(inspector, b, "sun transform");
    rotationOf(treeA).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});
    rotationOf(treeB).onChanged(ctx, Vec3{0.0f, 10.0f, 0.0f});

    auto rebuiltA = buildTurned(inspector, a, "lamp transform");
    auto rebuiltB = buildTurned(inspector, b, "sun transform");
    EXPECT_EQ(rotationOf(rebuiltA).v.y, 370.0f);
    EXPECT_EQ(rotationOf(rebuiltB).v.y, 10.0f);
}

TEST(EulerInspector, AnEditToAGoneTargetIsDroppedAndNotRemembered) {
    EulerInspector inspector;
    EulerTurned turned;
    EulerTurned* current = &turned;
    UIContext ctx;
    auto tree = buildPropertyTree(
        "Turned",
        PropertyTarget(
            [&current]() -> const void* { return current; },
            [&current]() -> void* { return current; },
            *getDesc<EulerTurned>()
        ),
        [] {},
        inspector.Overrides()
    );

    current = nullptr;
    rotationOf(tree).onChanged(ctx, Vec3{0.0f, 370.0f, 0.0f});
    current = &turned;

    auto rebuilt = buildTurned(inspector, turned);
    EXPECT_EQ(rotationOf(rebuilt).v, (Vec3{}));
}

TEST(EulerInspector, AnyOtherVec4ShowsAsItDoes) {
    EulerInspector inspector;
    EulerTurned turned;
    UIContext ctx;

    auto tree = buildTurned(inspector, turned);
    auto& tint = std::get<Float4Field>(std::get<Collapsing>(tree).children[1]);
    EXPECT_EQ(tint.v, turned.tint);

    tint.onChanged(ctx, Vec4{1.0f, 2.0f, 3.0f, 4.0f});
    EXPECT_EQ(turned.tint, (Vec4{1.0f, 2.0f, 3.0f, 4.0f}));
}
