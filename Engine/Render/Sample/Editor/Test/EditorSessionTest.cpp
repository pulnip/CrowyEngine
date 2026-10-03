#include <cmath>
#include <numbers>
#include <vector>

#include <gtest/gtest.h>

#include "ClassRegistry.hpp"
#include "DOM.hpp"
#include "EditorSession.hpp"

using namespace Crowy;

namespace
{
    constexpr f32 HalfPi = std::numbers::pi_v<f32> / 2;

    // two cuts and two keys; remembers what it was asked to apply
    class FakeContent final: public EditorContent {
    public:
        std::vector<EditorCut> cuts{
            EditorCut{.name = "wide", .position = {0.0f, 10.0f, -20.0f}, .yaw = 0.0f, .pitch = 0.3f},
            EditorCut{
                .name = "plan",
                .position = {0.0f, 120.0f, 0.0f},
                .pitch = HalfPi,
                .lens = {.orthographic = true, .orthoHalfHeight = 52.0f}
            }
        };
        std::vector<Str> keys{"day", "night"};
        std::vector<Str> applied;

        std::vector<EditorObject> objects{
            EditorObject{.name = "instance/lamp", .group = "Street", .detail = "Lamp"},
            EditorObject{.name = "instance/tower", .group = "NE", .detail = "Box"},
            EditorObject{.name = "light/lamp", .group = "Lights", .detail = "spot", .kind = EditorObjectKind::Light},
        };

        std::span<const EditorCut> Cuts() const override { return cuts; }
        std::span<const Str> Keys() const override { return keys; }
        Color ApplyKey(StrView key) override {
            applied.emplace_back(key);
            return key == "day" ? Color{0.5f, 0.6f, 0.7f, 1.0f} : Color{0.0f, 0.0f, 0.1f, 1.0f};
        }
        EditorObjects Objects() const override { return objects; }
        std::optional<usize> ObjectOf(PrimitiveHandle) const override { return std::nullopt; }
        std::optional<PrimitiveHandle> PrimitiveOf(usize) const override { return std::nullopt; }
        std::optional<LightHandle> LightOf(usize) const override { return std::nullopt; }
        MeshList MeshesOf(PrimitiveHandle) const override { return {}; }
        InspectSections Inspect(usize object) override {
            return {InspectSection{.label = "row", .target = &rows[object], .desc = GetDesc<EditorState>(), .apply = [this] { ++applies; }}};
        }

        // stands in for the content's rows
        std::vector<EditorState> rows{3};
        u32 applies = 0;
    };

    class Fixture {
    public:
        FakeContent content;
        RenderScene scene;
        EditorCamera camera{content.cuts.front()};
        Color clear{};
        EditorSession session{camera, content, scene, [this](Color color) { clear = color; }};

        Fixture() { session.Start("wide", "day"); }
    };

    Vec4 project(const Mat4& m, Vec3 p) {
        const auto clip = m * Vec4{p.x, p.y, p.z, 1.0f};

        return clip / clip.w;
    }
}

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

// the selection's section is exposed as `selection`, re-pointed on every
// selection; a port write applies and dirties the inspector
TEST(EditorSession, TheSelectionIsExposedAndRepointed) {
    FakeContent content;
    RenderScene scene;
    EditorCamera camera{content.cuts.front()};
    std::vector<Str> exposed;
    DirtyCallback portWrite;
    void* target = nullptr;
    EditorSession session{
        camera,
        content,
        scene,
        [](Color) {},
        EditorPort{
            .expose = [&](StrView name, void* row, const TypeDesc&, DirtyCallback onDirty) {
                exposed.emplace_back(name);
                target = row;
                portWrite = std::move(onDirty);
            },
            .unexpose = [&](StrView name) { std::erase(exposed, Str(name)); }
        }
    };
    session.Start("wide", "day");

    session.Select(0);
    ASSERT_EQ(exposed, std::vector<Str>{"selection"});
    EXPECT_EQ(target, &content.rows[0]);

    session.Select(2);
    EXPECT_EQ(exposed, std::vector<Str>{"selection"});
    EXPECT_EQ(target, &content.rows[2]);

    portWrite();
    EXPECT_EQ(content.applies, 1u);
    EXPECT_TRUE(session.TakeInspectorDirty());

    session.Select(std::nullopt);
    EXPECT_TRUE(exposed.empty());
}

