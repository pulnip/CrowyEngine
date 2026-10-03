#include <array>
#include <cmath>
#include <functional>
#include <map>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "DOM.hpp"
#include "EditorSession.hpp"
#include "Object.hpp"

using namespace Crowy;

namespace
{
    constexpr f32 HalfPi = std::numbers::pi_v<f32> / 2;
    constexpr Vec2 Window{1920.0f, 1080.0f};

    // a row the gizmo can move, turn and scale
    struct FakeRow {
        Vec3 position{};
        f32 yaw = 0.0f;
        Vec3 scale{1.0f, 1.0f, 1.0f};
    };

    void expectNear(Vec3 actual, Vec3 expected, f32 tolerance) {
        EXPECT_NEAR(actual.x, expected.x, tolerance);
        EXPECT_NEAR(actual.y, expected.y, tolerance);
        EXPECT_NEAR(actual.z, expected.z, tolerance);
    }

    Vec4 project(const Mat4& m, Vec3 p) {
        const auto clip = m * Vec4{p.x, p.y, p.z, 1.0f};

        return clip / clip.w;
    }

    // three cuts, two keys and three objects: a row, a box scaled per axis,
    // and a light whose section has no apply; remembers what it applied
    class FakeContent final: public EditorContent {
    public:
        std::vector<EditorCut> cuts{
            EditorCut{.name = "wide", .position = {0.0f, 10.0f, -20.0f}, .yaw = 0.0f, .pitch = 0.3f},
            EditorCut{
                .name = "plan",
                .position = {0.0f, 120.0f, 0.0f},
                .pitch = HalfPi,
                .lens = {.orthographic = true, .orthoHalfHeight = 52.0f}
            },
            // 30 degrees above the origin at 10 m: an arrow there is 0.8660254 m
            EditorCut{
                .name = "down",
                .position = {0.0f, 5.0f, -8.660254f},
                .pitch = HalfPi / 3,
                .lens = {.fovY = 2 * HalfPi / 3}
            }
        };
        std::vector<Str> keys{"day", "night"};
        std::vector<Str> applied;

        std::vector<EditorObject> objects{
            EditorObject{.name = "instance/lamp", .group = "Street", .detail = "Lamp"},
            EditorObject{.name = "instance/tower", .group = "NE", .detail = "Box"},
            EditorObject{.name = "light/lamp", .group = "Lights", .detail = "spot", .kind = EditorObjectKind::Light},
        };
        std::optional<LightHandle> lamp;
        // stands in for the content's rows
        std::vector<FakeRow> rows{3};
        u32 applies = 0;

        std::span<const EditorCut> Cuts() const override { return cuts; }
        std::span<const Str> Keys() const override { return keys; }
        Color ApplyKey(StrView key) override {
            applied.emplace_back(key);
            return key == "day" ? Color{0.5f, 0.6f, 0.7f, 1.0f} : Color{0.0f, 0.0f, 0.1f, 1.0f};
        }
        EditorObjects Objects() const override { return objects; }
        std::optional<usize> ObjectOf(PrimitiveHandle) const override { return std::nullopt; }
        std::optional<PrimitiveHandle> PrimitiveOf(usize) const override { return std::nullopt; }
        std::optional<LightHandle> LightOf(usize object) const override {
            return object == 2 ? lamp : std::nullopt;
        }
        MeshList MeshesOf(PrimitiveHandle) const override { return {}; }
        InspectSections Inspect(usize object) override {
            DirtyCallback apply;
            if(object != 2)
                apply = [this] { ++applies; };
            constexpr std::array Parts{
                combine(GizmoParts::Move, GizmoParts::Turn, GizmoParts::Scale),
                combine(GizmoParts::Move, GizmoParts::Turn, GizmoParts::ScaleAxes),
                GizmoParts::Move
            };

            return {InspectSection{
                .label = "row",
                .target = &rows[object],
                .desc = GetDesc<FakeRow>(),
                .apply = apply,
                .gizmo = Parts[object]
            }};
        }
    };

    // what the session exposed, by name, with each target's callback
    class FakePort {
    public:
        struct Exposure {
            void* target = nullptr;
            DirtyCallback onDirty;
        };

        std::map<Str, Exposure, std::less<>> exposures;

        EditorPort Bind() {
            return EditorPort{
                .expose = [this](StrView name, void* target, const TypeDesc&, DirtyCallback onDirty) {
                    exposures.insert_or_assign(Str(name), Exposure{.target = target, .onDirty = std::move(onDirty)});
                },
                .unexpose = [this](StrView name) { exposures.erase(Str(name)); }
            };
        }
    };

    class Fixture {
    public:
        // right of the wide cut's view, so a pick at the center misses it
        static constexpr Vec3 LampPosition{8.0f, 10.0f, 0.0f};

        FakeContent content;
        FakePort port;
        RenderScene scene;
        EditorCamera camera{content.cuts.front()};
        Color clear{};
        bool chrome = true;
        EditorSession session{camera, content, scene, chrome, [this](Color color) { clear = color; }, port.Bind()};

        Fixture() {
            content.lamp = scene.Lights().Add(LightSnapshot{.position = LampPosition});
            session.Start("wide", "day");
        }

        // the down cut with `object` selected, its gizmo measured
        void Hold(usize object) {
            session.State().cut = "down";
            session.Sync();
            session.SetViewport(Window);
            session.Select(object);
        }

        void Write() {
            port.exposures.at("editor").onDirty();
        }
    };
}

CROWY_STRUCT(FakeRow)
    .SetProperty("position", &FakeRow::position)
    .SetProperty("yaw", &FakeRow::yaw)
    .SetProperty("scale", &FakeRow::scale)
CROWY_STRUCT_END(FakeRow)

TEST(EditorCamera, PlanCutLooksStraightDownWithNorthUp) {
    FakeContent content;
    EditorCamera camera{content.cuts[1]};

    const auto forward = camera.Forward();
    EXPECT_NEAR(forward.y, -1.0f, 1e-5f);
    // 16:9, half height 52: the frame's top edge is 52 m north, its right
    // edge 52 * 16/9 m east
    const auto viewProj = camera.ViewProj(16.0f / 9.0f);
    EXPECT_NEAR(project(viewProj, {0.0f, 0.0f, 52.0f}).y, 1.0f, 1e-4f);
    EXPECT_NEAR(project(viewProj, {52.0f * 16.0f / 9.0f, 0.0f, 0.0f}).x, 1.0f, 1e-4f);
}

TEST(EditorCamera, PerspectiveCutKeepsItsFieldOfView) {
    FakeContent content;
    content.cuts[0].lens.fovY = 40.0f * std::numbers::pi_v<f32> / 180.0f;
    EditorCamera camera{content.cuts[0]};

    const auto projection = camera.Projection(16.0f / 9.0f);
    EXPECT_NEAR(projection[1][1], 1.0f / std::tan(20.0f * std::numbers::pi_v<f32> / 180.0f), 1e-4f);
    // positive pitch looks down
    EXPECT_LT(camera.Forward().y, 0.0f);
}

TEST(EditorSession, AWrittenCutSnapsTheCamera) {
    Fixture f;
    f.session.State().cut = "plan";
    f.session.Sync();

    EXPECT_EQ(f.session.State().cut, "plan");
    EXPECT_FLOAT_EQ(f.camera.position.y, 120.0f);
    EXPECT_TRUE(f.camera.lens.orthographic);
}

TEST(EditorSession, AnUnknownCutIsRevertedWithTheChoices) {
    Fixture f;
    f.session.State().cut = "nope";
    f.session.Sync();

    EXPECT_EQ(f.session.State().cut, "wide");
    EXPECT_NE(f.session.State().status.find("wide, plan"), Str::npos);
    EXPECT_FLOAT_EQ(f.camera.position.y, 10.0f);
}

TEST(EditorSession, AKeyIsAppliedOnceAndItsSkyCleared) {
    Fixture f;
    ASSERT_EQ(f.content.applied.size(), 1u);

    f.session.State().key = "night";
    f.session.Sync();
    EXPECT_EQ(f.content.applied.size(), 2u);
    EXPECT_FLOAT_EQ(f.clear.z, 0.1f);

    // the same value again applies nothing
    f.session.Sync();
    EXPECT_EQ(f.content.applied.size(), 2u);

    f.session.State().key = "dusk";
    f.session.Sync();
    EXPECT_EQ(f.session.State().key, "night");
    EXPECT_EQ(f.content.applied.size(), 2u);
}

TEST(EditorSession, AWrittenStatusIsReverted) {
    Fixture f;
    const auto before = f.session.State().status;
    f.session.State().status = "hello";
    f.session.Sync();

    EXPECT_EQ(f.session.State().status, before);
}

// the port's path: resolve the property, deserialize into it, then the
// target's callback, which is Sync
TEST(EditorSession, ThePortsWriteBehavesAsADirectOne) {
    Fixture f;
    const auto* desc = GetDesc<EditorState>();
    ASSERT_NE(desc, nullptr);
    auto resolved = ResolveProperty(&f.session.State(), *desc, "cut");
    ASSERT_NE(resolved.desc, nullptr) << resolved.error;
    ASSERT_TRUE(resolved.desc->type.deserialize(resolved.member, DOM::Value(Str("plan"))));
    f.session.Sync();

    EXPECT_FLOAT_EQ(f.camera.position.y, 120.0f);
}

// choosing the cut the camera already left snaps it back
TEST(EditorSession, TheSameCutAgainSnapsBack) {
    Fixture f;
    f.camera.position.y = 50.0f;
    f.session.SelectCut(0);

    EXPECT_FLOAT_EQ(f.camera.position.y, 10.0f);
    EXPECT_EQ(f.session.State().cut, "wide");
}

// a camera written over the port has left its cut, so the cut written again
// over the port snaps back, as the key for it does
TEST(EditorSession, APortCameraWriteFreesTheCut) {
    Fixture f;
    ASSERT_TRUE(f.port.exposures.contains("camera"));
    ASSERT_EQ(f.port.exposures.at("camera").target, &f.camera);
    f.camera.position.y = 50.0f;
    f.port.exposures.at("camera").onDirty();
    EXPECT_EQ(f.session.State().cut, "free");

    f.session.State().cut = "wide";
    f.port.exposures.at("editor").onDirty();
    EXPECT_FLOAT_EQ(f.camera.position.y, 10.0f);
    EXPECT_EQ(f.session.State().cut, "wide");
}

TEST(EditorSession, AFullNameSelects) {
    Fixture f;
    f.session.State().selected = "instance/tower";
    f.session.Sync();

    EXPECT_EQ(f.session.Selection(), 1u);
    EXPECT_EQ(f.session.State().selected, "instance/tower");
}

// a bare name one object carries is written back qualified; one that two
// objects carry is refused
TEST(EditorSession, ABareNameSelectsOnlyWhenUnique) {
    Fixture f;
    f.session.State().selected = "tower";
    f.session.Sync();
    EXPECT_EQ(f.session.State().selected, "instance/tower");

    f.session.State().selected = "lamp";
    f.session.Sync();
    EXPECT_EQ(f.session.State().selected, "instance/tower");
    EXPECT_NE(f.session.State().status.find("more than one"), Str::npos);
}

TEST(EditorSession, AnUnknownNameIsRevertedAndEmptyClears) {
    Fixture f;
    f.session.Select(0);
    f.session.State().selected = "instance/nope";
    f.session.Sync();
    EXPECT_EQ(f.session.State().selected, "instance/lamp");

    f.session.State().selected = "";
    f.session.Sync();
    EXPECT_FALSE(f.session.Selection().has_value());
}

// a pick that hits nothing clears the selection, from the port as from a click
TEST(EditorSession, APickOnEmptySpaceClears) {
    Fixture f;
    f.session.Select(1);
    f.session.Update({1920.0f, 1080.0f});
    f.session.PickAt({960.0f, 540.0f});

    EXPECT_FALSE(f.session.Selection().has_value());
    EXPECT_EQ(f.session.State().selected, "");
}

// a write picks every time, so the same pixel written again picks again
TEST(EditorSession, PickAtIsAPulse) {
    Fixture f;
    f.session.Update({1920.0f, 1080.0f});
    for(int write = 0; write < 2; ++write) {
        f.session.Select(1);
        f.session.State().pickAt = Vec2{960.0f, 540.0f};
        f.port.exposures.at("editor").onDirty();

        EXPECT_FALSE(f.session.Selection().has_value()) << "write " << write;
        EXPECT_EQ(f.session.State().pickAt, EditorNoPick);
    }
}

// the selection's section is exposed as `selection`, re-pointed on every
// selection; a port write applies and dirties the inspector
TEST(EditorSession, TheSelectionIsExposedAndRepointed) {
    Fixture f;
    EXPECT_FALSE(f.port.exposures.contains("selection"));

    f.session.Select(0);
    ASSERT_TRUE(f.port.exposures.contains("selection"));
    EXPECT_EQ(f.port.exposures.at("selection").target, &f.content.rows[0]);

    f.session.Select(1);
    EXPECT_EQ(f.port.exposures.size(), 4u);
    EXPECT_EQ(f.port.exposures.at("selection").target, &f.content.rows[1]);

    f.session.TakeInspectorDirty();
    f.port.exposures.at("selection").onDirty();
    EXPECT_EQ(f.content.applies, 1u);
    EXPECT_TRUE(f.session.TakeInspectorDirty());

    f.session.Select(std::nullopt);
    EXPECT_FALSE(f.port.exposures.contains("selection"));
}

// the destructor takes `editor`, `camera` and the selection off the port
TEST(EditorSession, ItsTargetsLeaveThePortWithIt) {
    FakeContent content;
    FakePort port;
    RenderScene scene;
    EditorCamera camera{content.cuts.front()};
    {
        const bool chrome = true;
        EditorSession session{camera, content, scene, chrome, [](Color) {}, port.Bind()};
        session.Start("wide", "day");
        session.Select(0);
        EXPECT_EQ(port.exposures.size(), 4u);
    }
    EXPECT_TRUE(port.exposures.empty());
}


// a click near a light's marker selects the light ahead of geometry; the
// viewport set before any frame is the one picks are measured in
TEST(EditorSession, AMarkerNearTheClickSelectsItsLight) {
    Fixture f;
    const Vec2 viewport{1920.0f, 1080.0f};
    f.session.SetViewport(viewport);
    const auto marker = projectToWindow(f.camera.ViewProj(viewport.x / viewport.y), Fixture::LampPosition, viewport);
    ASSERT_TRUE(marker.has_value());

    f.session.PickAt(*marker + Vec2{5.0f, 3.0f});
    EXPECT_EQ(f.session.Selection(), 2u);
    EXPECT_EQ(f.session.State().selected, "light/lamp");

    f.session.PickAt(*marker + Vec2{20.0f, 0.0f});
    EXPECT_FALSE(f.session.Selection().has_value());
}

// a key rewrites rows the inspector may show, so the panel rebuilds
TEST(EditorSession, AKeyChangeDirtiesTheInspector) {
    Fixture f;
    f.session.Select(0);
    f.session.TakeInspectorDirty();

    f.session.State().key = "night";
    f.session.Sync();
    EXPECT_TRUE(f.session.TakeInspectorDirty());
}

// a section the content gives no apply gets one that does nothing, so the
// panel's widgets and the port can always call it
TEST(EditorSession, ASectionWithoutApplyIsSafeToWrite) {
    Fixture f;
    f.session.Select(2);
    ASSERT_EQ(f.session.Inspected().size(), 1u);
    ASSERT_TRUE(static_cast<bool>(f.session.Inspected()[0].apply));

    EXPECT_NO_THROW(f.session.Inspected()[0].apply());
    EXPECT_NO_THROW(f.port.exposures.at("selection").onDirty());
    EXPECT_EQ(f.content.applies, 0u);
}

// a port script presses the X arrow where a person sees it and drags it to
// its reach: one meter, through the selection's apply and the panel's rebuild
TEST(EditorGizmo, APortGrabAndDragWriteTheRow) {
    Fixture f;
    f.Hold(0);
    const auto aim = f.session.Gizmo().moveX;
    EXPECT_NEAR(aim.grab.x, 1041.0f, 0.05f);
    EXPECT_NEAR(aim.grab.y, 540.0f, 0.05f);
    f.session.TakeInspectorDirty();

    f.session.State().grab = aim.grab;
    f.Write();
    EXPECT_EQ(f.session.State().handle, GizmoHandle::MoveX);
    EXPECT_EQ(f.session.State().grab, EditorNoPick);

    f.session.State().drag = aim.reach;
    f.Write();
    expectNear(f.content.rows[0].position, {1.0f, 0.0f, 0.0f}, 1e-4f);
    EXPECT_EQ(f.content.applies, 1u);
    EXPECT_TRUE(f.session.TakeInspectorDirty());
    EXPECT_EQ(f.session.State().drag, EditorNoPick);

    // measured from the grab, the same pixel again leaves the meter
    f.session.State().drag = aim.reach;
    f.Write();
    EXPECT_NEAR(f.content.rows[0].position.x, 1.0f, 1e-4f);
    EXPECT_EQ(f.content.applies, 2u);
}

TEST(EditorGizmo, TheMouseAndThePortLeaveTheSameRow) {
    const auto perMeter = 81.0f / 0.8660254f;

    Fixture hand;
    hand.Hold(0);
    const auto tip = hand.session.Gizmo().moveX.grab;
    const auto to = tip + Vec2{0.9f * perMeter, 0.0f};
    ASSERT_TRUE(hand.session.Grab(tip));
    hand.session.DragTo(to, true);
    hand.session.Release();
    EXPECT_FLOAT_EQ(hand.content.rows[0].position.x, 1.0f);
    EXPECT_EQ(hand.session.Held(), GizmoHandle::None);

    Fixture script;
    script.Hold(0);
    script.session.State().snap = true;
    script.session.State().grab = tip;
    script.Write();
    script.session.State().drag = to;
    script.Write();
    script.session.State().handle = GizmoHandle::None;
    script.Write();
    EXPECT_EQ(script.content.rows[0].position, hand.content.rows[0].position);
}

// None lets go where the drag stands; a handle no press took is refused;
// cancel puts the row back bit for bit
TEST(EditorGizmo, ReleaseAndCancel) {
    Fixture f;
    f.Hold(0);
    const auto aim = f.session.Gizmo().moveX;

    f.session.State().handle = GizmoHandle::MoveZ;
    f.Write();
    EXPECT_EQ(f.session.State().handle, GizmoHandle::None);
    EXPECT_NE(f.session.State().status.find("editor.grab"), Str::npos);

    ASSERT_TRUE(f.session.Grab(aim.grab));
    f.session.DragTo(aim.reach, false);
    f.session.State().handle = GizmoHandle::None;
    f.Write();
    f.session.State().drag = aim.reach + Vec2{100.0f, 0.0f};
    f.Write();
    EXPECT_NEAR(f.content.rows[0].position.x, 1.0f, 1e-4f);
    EXPECT_NE(f.session.State().status.find("editor.grab"), Str::npos);

    const auto start = f.content.rows[0].position;
    ASSERT_TRUE(f.session.Grab(f.session.Gizmo().moveX.grab));
    f.session.DragTo(f.session.Gizmo().moveX.reach + Vec2{50.0f, 0.0f}, false);
    EXPECT_NE(f.content.rows[0].position, start);
    f.session.State().cancel = true;
    f.Write();
    EXPECT_EQ(f.content.rows[0].position, start);
    EXPECT_EQ(f.session.Held(), GizmoHandle::None);
    EXPECT_FALSE(f.session.State().cancel);
}

TEST(EditorGizmo, TheGizmoLivesWithTheChrome) {
    Fixture f;
    f.chrome = false;
    f.Hold(0);
    EXPECT_EQ(f.session.Gizmo().pivot, EditorNoPick);
    f.session.State().grab = {1041.0f, 540.0f};
    f.Write();
    EXPECT_EQ(f.session.Held(), GizmoHandle::None);
    EXPECT_NE(f.session.State().status.find("debug.showPanel"), Str::npos);

    f.chrome = true;
    f.session.Update(Window);
    ASSERT_TRUE(f.session.Grab(f.session.Gizmo().moveX.grab));
    f.chrome = false;
    f.session.Update(Window);
    EXPECT_EQ(f.session.Held(), GizmoHandle::None);
    EXPECT_EQ(f.session.State().handle, GizmoHandle::None);
}

TEST(EditorGizmo, AnotherSelectionLetsGo) {
    Fixture f;
    f.Hold(0);
    ASSERT_TRUE(f.session.Grab(f.session.Gizmo().moveX.grab));
    f.session.Select(1);
    EXPECT_EQ(f.session.Held(), GizmoHandle::None);

    f.session.State().drag = {1200.0f, 540.0f};
    f.Write();
    EXPECT_EQ(f.content.rows[0].position, Vec3{});
    EXPECT_EQ(f.content.rows[1].position, Vec3{});
}

TEST(EditorGizmo, ALightMovesOnlyAndABoxScalesPerAxis) {
    Fixture f;
    f.Hold(2);
    EXPECT_NE(f.session.Gizmo().moveX.grab, EditorNoPick);
    EXPECT_EQ(f.session.Gizmo().ring.grab, EditorNoPick);
    EXPECT_EQ(f.session.Gizmo().scale.grab, EditorNoPick);

    f.session.Select(1);
    EXPECT_EQ(f.session.Gizmo().scale.grab, EditorNoPick);
    const auto aim = f.session.Gizmo().scaleX;
    ASSERT_NE(aim.grab, EditorNoPick);
    ASSERT_TRUE(f.session.Grab(aim.grab));
    f.session.DragTo(aim.reach, false);
    expectNear(f.content.rows[1].scale, {2.0f, 1.0f, 1.0f}, 1e-3f);
}

TEST(EditorGizmo, TheRingWritesAWrappedYaw) {
    Fixture f;
    f.content.rows[0].yaw = 300.0f;
    f.Hold(0);
    const auto aim = f.session.Gizmo().ring;
    ASSERT_TRUE(f.session.Grab(aim.grab));
    EXPECT_EQ(f.session.Held(), GizmoHandle::Ring);
    f.session.DragTo(aim.reach, false);
    EXPECT_NEAR(f.content.rows[0].yaw, 30.0f, 1e-3f);
}

TEST(EditorGizmo, AWriteToTheGizmoViewIsPutBack) {
    Fixture f;
    f.Hold(0);
    auto& exposure = f.port.exposures.at("gizmo");
    static_cast<GizmoView*>(exposure.target)->moveX.grab = {5.0f, 5.0f};
    exposure.onDirty();

    EXPECT_NEAR(f.session.Gizmo().moveX.grab.x, 1041.0f, 0.05f);
}
