#include <cmath>
#include <limits>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "EditorGizmo.hpp"

using namespace Crowy;

namespace
{
    using MakeCamera = EditorCamera (*)();

    constexpr Vec2 Viewport{1920.0f, 1080.0f};
    constexpr f32 Pi = std::numbers::pi_v<f32>;
    constexpr auto AllParts = combine(GizmoParts::Move, GizmoParts::Turn, GizmoParts::Scale);
    constexpr auto BoxParts = combine(GizmoParts::Move, GizmoParts::Turn, GizmoParts::ScaleAxes);

    usize at(GizmoHandle handle) {
        return static_cast<usize>(handle);
    }

    f32 radians(f32 degrees) {
        return degrees * Pi / 180.0f;
    }

    // 30 degrees above the origin at 10 m, which lands at the center: 81
    // points of arrow are 0.8660254 m, 93.5307 points a meter
    EditorCamera downCamera() {
        return EditorCamera{EditorCut{.name = "down", .position = {0.0f, 5.0f, -8.660254f}, .pitch = Pi / 6, .lens = {.fovY = Pi / 3}}};
    }

    EditorCamera levelCamera() {
        return EditorCamera{EditorCut{.name = "level", .lens = {.fovY = Pi / 3}}};
    }

    EditorCamera planCamera() {
        return EditorCamera{EditorCut{
            .name = "plan",
            .position = {0.0f, 120.0f, 0.0f},
            .pitch = Pi / 2,
            .lens = {.orthographic = true, .orthoHalfHeight = 52.0f}
        }};
    }

    // Backlot's street cut in the engine's signs
    EditorCamera streetCamera() {
        return EditorCamera{EditorCut{
            .name = "street",
            .position = {-9.0f, 1.6f, -9.0f},
            .yaw = Pi / 4,
            .pitch = -radians(4.0f),
            .lens = {.fovY = radians(50.0f), .nearZ = 0.71f}
        }};
    }

    GizmoLayout layoutOf(const EditorCamera& camera, Vec3 pivot, f32 yaw = 0.0f, GizmoParts parts = AllParts) {
        return layoutGizmo(camera, Viewport, pivot, yaw, parts).value();
    }

    Vec2 pixelOf(const GizmoLayout& layout, Vec3 point) {
        return projectToWindow(layout.viewProj, point, Viewport).value();
    }

    Ray3D rayAt(const EditorCamera& camera, Vec2 pixel) {
        return rayThroughPixel(camera, pixel, Viewport).value();
    }

    // pressed at the handle's grab aim and dragged to `pixel`
    std::optional<GizmoEdit> dragTo(const EditorCamera& camera, const GizmoLayout& layout, GizmoHandle handle, Vec2 pixel, bool snap = false) {
        const auto aim = aimGizmo(layout, handle).value();
        const auto drag = grabGizmo(layout, handle, rayAt(camera, aim.grab)).value();

        return dragGizmo(drag, rayAt(camera, pixel), snap);
    }

    void expectNear(Vec3 actual, Vec3 expected, f32 tolerance) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }
}

TEST(GizmoLayout, ArrowsKeepTheirLengthOnScreen) {
    const auto down = downCamera();
    const auto layout = layoutOf(down, {0.0f, 0.0f, 0.0f});
    EXPECT_NEAR(layout.length, 0.8660254f, 1e-5f);

    const auto screen = projectGizmo(layout);
    EXPECT_NEAR(screen.pivot.x, 960.0f, 0.05f);
    EXPECT_NEAR(screen.pivot.y, 540.0f, 0.05f);
    EXPECT_NEAR(screen.marks[at(GizmoHandle::MoveX)].to.x, 1041.0f, 0.05f);
    EXPECT_NEAR(screen.marks[at(GizmoHandle::MoveX)].to.y, 540.0f, 0.05f);

    // deeper and off the axis, the same 81 points
    const auto far = projectGizmo(layoutOf(down, {2.0f, 0.0f, 10.0f}));
    EXPECT_NEAR(norm(far.marks[at(GizmoHandle::MoveX)].to - far.pivot), 81.0f, 0.05f);

    const auto planLayout = layoutOf(planCamera(), {0.0f, 0.0f, 0.0f});
    EXPECT_NEAR(planLayout.length, 7.8f, 1e-5f);
    const auto plan = projectGizmo(planLayout);
    EXPECT_NEAR(norm(plan.marks[at(GizmoHandle::MoveX)].to - plan.pivot), 81.0f, 0.05f);
}

TEST(GizmoLayout, AnArrowSeenEndOnIsHidden) {
    const auto level = levelCamera();
    const auto ahead = layoutOf(level, {0.0f, 0.0f, 10.0f});
    EXPECT_FALSE(ahead.shown[at(GizmoHandle::MoveZ)]);
    EXPECT_TRUE(ahead.shown[at(GizmoHandle::MoveX)]);
    EXPECT_TRUE(ahead.shown[at(GizmoHandle::MoveY)]);

    // 5 degrees off the line of sight is still end-on, 9 is not
    EXPECT_FALSE(layoutOf(level, {0.8715574f, 0.0f, 9.9619470f}).shown[at(GizmoHandle::MoveZ)]);
    EXPECT_TRUE(layoutOf(level, {1.5643447f, 0.0f, 9.8768834f}).shown[at(GizmoHandle::MoveZ)]);

    const auto plan = layoutOf(planCamera(), {0.0f, 0.0f, 0.0f});
    EXPECT_FALSE(plan.shown[at(GizmoHandle::MoveY)]);
    EXPECT_TRUE(plan.shown[at(GizmoHandle::MoveZ)]);
}

// the ring reaches 1.25 arrows toward the eye: 0.33 m away it crosses the
// 0.3 m near plane, 0.34 m away it does not
TEST(GizmoLayout, NoneBehindTheEyeOrAtTheNearPlane) {
    const auto level = levelCamera();
    EXPECT_FALSE(layoutGizmo(level, Viewport, {0.0f, 0.0f, -5.0f}, 0.0f, AllParts));
    EXPECT_FALSE(layoutGizmo(level, Viewport, {0.0f, 0.0f, 0.33f}, 0.0f, AllParts));
    EXPECT_TRUE(layoutGizmo(level, Viewport, {0.0f, 0.0f, 0.34f}, 0.0f, AllParts));
    EXPECT_FALSE(layoutGizmo(planCamera(), Viewport, {0.0f, 130.0f, 0.0f}, 0.0f, AllParts));
    EXPECT_FALSE(layoutGizmo(level, {0.0f, 0.0f}, {0.0f, 0.0f, 10.0f}, 0.0f, AllParts));
}

TEST(GizmoHover, TheNearestHandleWithinSixPoints) {
    const auto layout = layoutOf(downCamera(), {0.0f, 0.0f, 0.0f});
    const auto screen = projectGizmo(layout);

    EXPECT_EQ(hoverGizmo(screen, {1000.0f, 540.0f}), GizmoHandle::MoveX);
    EXPECT_EQ(hoverGizmo(screen, {1000.0f, 545.9f}), GizmoHandle::MoveX);
    EXPECT_EQ(hoverGizmo(screen, {1000.0f, 546.1f}), GizmoHandle::None);
    // the ring's +X end, 20 points past the arrow's tip
    EXPECT_EQ(hoverGizmo(screen, {1061.25f, 540.0f}), GizmoHandle::Ring);
    const auto square = aimGizmo(layout, GizmoHandle::Scale).value().grab;
    EXPECT_EQ(hoverGizmo(screen, square + Vec2{4.0f, -4.0f}), GizmoHandle::Scale);
    // the arrows start a fifth of the way out, so the pivot itself picks
    EXPECT_EQ(hoverGizmo(screen, {960.0f, 540.0f}), GizmoHandle::None);
}

TEST(GizmoHover, ATieGoesToTheEarlierHandle) {
    GizmoScreen screen;
    screen.marks[at(GizmoHandle::MoveX)] = GizmoMark{.shown = true, .from = {0.0f, 0.0f}, .to = {100.0f, 0.0f}};
    screen.marks[at(GizmoHandle::MoveY)] = GizmoMark{.shown = true, .from = {0.0f, 8.0f}, .to = {100.0f, 8.0f}};

    EXPECT_EQ(hoverGizmo(screen, {50.0f, 3.0f}), GizmoHandle::MoveX);
    EXPECT_EQ(hoverGizmo(screen, {50.0f, 4.0f}), GizmoHandle::MoveX);
    EXPECT_EQ(hoverGizmo(screen, {50.0f, 5.0f}), GizmoHandle::MoveY);
}

TEST(GizmoDrag, AnAxisDragMovesByTheAnalyticDistance) {
    const auto down = downCamera();
    const auto layout = layoutOf(down, {0.0f, 0.0f, 0.0f});
    const auto perMeter = 81.0f / 0.8660254f;
    const auto tip = aimGizmo(layout, GizmoHandle::MoveX).value().grab;

    const auto raw = dragTo(down, layout, GizmoHandle::MoveX, tip + Vec2{0.9f * perMeter, 0.0f}).value();
    EXPECT_NEAR(raw.offset.x, 0.9f, 1e-3f);
    EXPECT_EQ(raw.offset.y, 0.0f);
    EXPECT_EQ(raw.offset.z, 0.0f);

    // Ctrl snaps the change: 0.9 rounds to 1.0 and 0.8 to 0.75
    EXPECT_FLOAT_EQ(dragTo(down, layout, GizmoHandle::MoveX, tip + Vec2{0.9f * perMeter, 0.0f}, true).value().offset.x, 1.0f);
    EXPECT_FLOAT_EQ(dragTo(down, layout, GizmoHandle::MoveX, tip + Vec2{0.8f * perMeter, 0.0f}, true).value().offset.x, 0.75f);
    // the cursor off the axis moves along it all the same
    EXPECT_NEAR(dragTo(down, layout, GizmoHandle::MoveX, tip + Vec2{0.9f * perMeter, 40.0f}).value().offset.x, 0.9f, 1e-2f);
}

TEST(GizmoDrag, AQuarterOfTheRingIsNinetyDegrees) {
    const auto down = downCamera();
    const auto layout = layoutOf(down, {0.0f, 0.0f, 0.0f});
    ASSERT_FALSE(layout.ringEdgeOn);
    const auto radius = GizmoRingRadius * layout.length;
    // grabbed at +X: the front (-Z) is a quarter turn clockwise seen from above
    const auto ring = [&](f32 degrees) {
        return pixelOf(layout, Vec3{std::sin(radians(degrees)), 0.0f, std::cos(radians(degrees))} * radius);
    };

    EXPECT_NEAR(dragTo(down, layout, GizmoHandle::Ring, ring(180.0f)).value().turn, 90.0f, 0.01f);
    EXPECT_NEAR(dragTo(down, layout, GizmoHandle::Ring, ring(0.0f)).value().turn, -90.0f, 0.01f);
    EXPECT_NEAR(dragTo(down, layout, GizmoHandle::Ring, ring(170.0f)).value().turn, 80.0f, 0.01f);
    EXPECT_FLOAT_EQ(dragTo(down, layout, GizmoHandle::Ring, ring(170.0f), true).value().turn, 75.0f);
}

// seen within 10 degrees of its plane the ring turns 90 degrees per radius
TEST(GizmoDrag, AnEdgeOnRingTurnsByTheDragsLength) {
    const auto level = levelCamera();
    const Vec3 pivot{0.0f, 0.0f, 10.0f};
    const auto layout = layoutOf(level, pivot);
    ASSERT_TRUE(layout.ringEdgeOn);
    const auto radius = GizmoRingRadius * layout.length;

    EXPECT_NEAR(dragTo(level, layout, GizmoHandle::Ring, pixelOf(layout, pivot)).value().turn, 90.0f, 0.01f);
    EXPECT_NEAR(dragTo(level, layout, GizmoHandle::Ring, pixelOf(layout, pivot + Vec3{2.0f * radius, 0.0f, 0.0f})).value().turn, -90.0f, 0.01f);
    EXPECT_NEAR(dragTo(level, layout, GizmoHandle::Ring, pixelOf(layout, pivot - Vec3{radius, 0.0f, 0.0f})).value().turn, 180.0f, 0.01f);

    // lamp-ne from the street cut sits 3.3 degrees below the eye
    EXPECT_TRUE(layoutOf(streetCamera(), {10.5f, 0.15f, 7.0f}, 180.0f).ringEdgeOn);
}

TEST(GizmoDrag, ScaleFloorsInsteadOfCrossingZero) {
    const auto down = downCamera();
    const auto layout = layoutOf(down, {0.0f, 0.0f, 0.0f});
    const auto aim = aimGizmo(layout, GizmoHandle::Scale).value();
    const auto pivot = projectGizmo(layout).pivot;

    expectNear(dragTo(down, layout, GizmoHandle::Scale, aim.reach).value().factor, {2.0f, 2.0f, 2.0f}, 1e-4f);
    EXPECT_FLOAT_EQ(dragTo(down, layout, GizmoHandle::Scale, pivot).value().factor.x, 0.01f);
    EXPECT_FLOAT_EQ(dragTo(down, layout, GizmoHandle::Scale, pivot, true).value().factor.x, 0.1f);
    EXPECT_FLOAT_EQ(dragTo(down, layout, GizmoHandle::Scale, pivot * 2.0f - aim.grab).value().factor.x, 0.01f);

    // the unit box scales one local axis, turned with its yaw
    const auto box = layoutOf(down, {3.0f, 0.15f, 1.0f}, 30.0f, BoxParts);
    EXPECT_FALSE(box.shown[at(GizmoHandle::Scale)]);
    const auto alongX = dragTo(down, box, GizmoHandle::ScaleX, aimGizmo(box, GizmoHandle::ScaleX).value().reach).value();
    expectNear(alongX.factor, {2.0f, 1.0f, 1.0f}, 1e-3f);
    const auto alongZ = dragTo(down, box, GizmoHandle::ScaleZ, aimGizmo(box, GizmoHandle::ScaleZ).value().reach).value();
    expectNear(alongZ.factor, {1.0f, 1.0f, 2.0f}, 1e-3f);
}

TEST(GizmoDrag, AParallelBackwardOrFarRayIsIgnored) {
    const auto level = levelCamera();
    const auto layout = layoutOf(level, {0.0f, 0.0f, 10.0f});
    const auto aim = aimGizmo(layout, GizmoHandle::MoveY).value();
    const auto drag = grabGizmo(layout, GizmoHandle::MoveY, rayAt(level, aim.grab)).value();
    expectNear(drag.normal, {0.0f, 0.0f, 1.0f}, 1e-5f);

    constexpr auto NaN = std::numeric_limits<f32>::quiet_NaN();
    const Vec3 eye{0.0f, 0.0f, 0.0f};
    EXPECT_FALSE(dragGizmo(drag, Ray3D{.origin = eye, .direction = {1.0f, 0.0f, 0.0f}}, false));
    EXPECT_FALSE(dragGizmo(drag, Ray3D{.origin = eye, .direction = {0.0f, 0.0f, -1.0f}}, false));
    // it meets the plane a million meters out
    EXPECT_FALSE(dragGizmo(drag, Ray3D{.origin = eye, .direction = normalize(Vec3{1.0f, 0.0f, 1e-5f})}, false));
    EXPECT_FALSE(dragGizmo(drag, Ray3D{.origin = eye, .direction = {NaN, NaN, NaN}}, false));
}

// a script presses at a handle's grab aim and lets go at its reach aim;
// that is one unit under every lens and pose, the street cut's included
TEST(GizmoDrag, EveryAimMakesOneUnit) {
    struct Case {
        MakeCamera camera;
        Vec3 pivot;
        f32 yaw;
        GizmoParts parts;
    };
    const std::vector<Case> cases{
        {downCamera, {0.0f, 0.0f, 0.0f}, 0.0f, AllParts},
        {downCamera, {3.0f, 0.15f, 1.0f}, 30.0f, BoxParts},
        {planCamera, {10.5f, 0.15f, 7.0f}, 180.0f, AllParts},
        {levelCamera, {0.0f, 0.0f, 10.0f}, 0.0f, AllParts},
        {streetCamera, {10.5f, 0.15f, 7.0f}, 180.0f, AllParts},
        {downCamera, {0.0f, 0.0f, 0.0f}, 0.0f, GizmoParts::Move},
    };

    for(usize c = 0; c < cases.size(); ++c) {
        const auto& [make, pivot, yaw, parts] = cases[c];
        const auto camera = make();
        const auto layout = layoutOf(camera, pivot, yaw, parts);
        const auto screen = projectGizmo(layout);
        usize shown = 0;
        for(usize i = 1; i < GizmoHandleCount; ++i) {
            const auto handle = static_cast<GizmoHandle>(i);
            const auto aim = aimGizmo(layout, handle);
            ASSERT_EQ(aim.has_value(), layout.shown[i]) << "case " << c << " handle " << i;
            if(!aim)
                continue;
            ++shown;

            EXPECT_EQ(hoverGizmo(screen, aim->grab), handle) << "case " << c << " handle " << i;
            const auto edit = dragTo(camera, layout, handle, aim->reach);
            ASSERT_TRUE(edit.has_value()) << "case " << c << " handle " << i;
            if(isGizmoArrow(handle)) {
                expectNear(edit->offset, gizmoAxis(layout, handle), 1e-3f);
            } else if(handle == GizmoHandle::Ring) {
                EXPECT_NEAR(edit->turn, 90.0f, 0.01f) << "case " << c;
            } else {
                const auto& f = edit->factor;
                const auto two = [&](GizmoHandle axis) { return handle == GizmoHandle::Scale || handle == axis ? 2.0f : 1.0f; };
                expectNear(f, {two(GizmoHandle::ScaleX), two(GizmoHandle::ScaleY), two(GizmoHandle::ScaleZ)}, 1e-3f);
            }
        }
        EXPECT_GT(shown, 0u) << "case " << c;
    }

    // a light moves only
    const auto light = layoutOf(downCamera(), {0.0f, 0.0f, 0.0f}, 0.0f, GizmoParts::Move);
    EXPECT_FALSE(aimGizmo(light, GizmoHandle::Ring));
    EXPECT_FALSE(aimGizmo(light, GizmoHandle::Scale));
}

TEST(GizmoMaths, YawWrapsAndSnaps) {
    EXPECT_FLOAT_EQ(wrapDegrees(370.0f), 10.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(-90.0f), 270.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(360.0f), 0.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(-360.0f), 0.0f);
    EXPECT_FLOAT_EQ(wrapDegrees(270.0f), 270.0f);
    const auto hair = wrapDegrees(-1e-6f);
    EXPECT_GE(hair, 0.0f);
    EXPECT_LT(hair, 360.0f);

    EXPECT_FLOAT_EQ(snapTo(22.0f, 15.0f), 15.0f);
    EXPECT_FLOAT_EQ(snapTo(-8.0f, 15.0f), -15.0f);
    EXPECT_FLOAT_EQ(snapTo(7.0f, 15.0f), 0.0f);
    EXPECT_FLOAT_EQ(snapTo(0.9f, 0.25f), 1.0f);
}
