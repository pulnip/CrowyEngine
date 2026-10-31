#include <numbers>
#include <optional>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "ExpectNear.hpp"
#include "Geometry/Projection.hpp"
#include "LightComponent.hpp"
#include "MeshAsset.hpp"
#include "MeshGenerator.hpp"
#include "MeshRowComponent.hpp"
#include "RenderCamera.hpp"
#include "RenderScene.hpp"
#include "WorldEditor.hpp"

using namespace Crowy;
using testing::HasSubstr;

namespace
{
    constexpr f32 Pi = std::numbers::pi_v<f32>;
    constexpr Vec2 View{1920.0f, 1080.0f};

    // 30 degrees above the origin at 10 m: the X arrow's aims there are a
    // meter apart
    EditorCut downCut() {
        return EditorCut{
            .name = "down",
            .position = {0.0f, 5.0f, -8.660254f},
            .pitch = Pi / 6,
            .lens = {.fovY = Pi / 3}
        };
    }

    MeshStore::Loaded loadBox(const AssetRef&) {
        MeshAsset asset;
        asset.slots.emplace_back().mesh = MakeBox(0.5f);

        return asset;
    }

    // a box at the origin, a sun and an unnamed empty beside it, landed by a
    // Frame; the stores outlive the World
    class WorldEditorTest: public testing::Test {
    protected:
        MeshStore meshes{&loadBox};
        RenderScene scene;
        CameraSnapshot snapshot;
        PrimitiveActors primitiveActors;
        World world;
        ActorId sun;
        ActorId box;
        ActorId empty;
        EditorCamera camera{downCut()};
        std::optional<WorldEditor> editor;

        void SetUp() override {
            world.Provide(scene);
            world.Provide(snapshot);
            world.Provide(primitiveActors);

            const auto mesh = scene.Meshes().Add(
                MeshResource{
                    .localBounds = {.center = zeros(), .halfScale = ones()},
                    .source =
                        meshes
                            .Load(AssetRef::Parse("engine:Builtin/Box").value())
                            .value()
                }
            );
            sun = spawn(
                "sun",
                Transform{
                    .position = {4.0f, 2.0f, 0.0f},
                    .rotation = quat(rotateY(-Pi / 6), rotateX(Pi / 3))
                }
            );
            world.Add(sun, LightComponent{});
            box = spawn("box", Transform{});
            world.Add(box, MeshRowComponent{mesh});
            empty = world.Spawn(Transform{.position = {-3.0f, 0.0f, 0.0f}});
            world.Frame(0.0);

            editor.emplace(world, scene, camera);
            editor->Update(View);
        }

        ActorId spawn(StrView name, const Transform& local) {
            const auto actor = world.Spawn(local);
            world.SetName(actor, Str(name));

            return actor;
        }

        Vec2 pointOf(Vec3 position) const {
            return projectToWindow(
                       camera.ViewProj(View.x / View.y),
                       position,
                       View
            )
                .value();
        }

        // the handle pressed at its grab aim and let go at its reach
        void dragOnce(GizmoAim GizmoAims::* handle) {
            const auto aim = editor->Gizmo().*handle;
            ASSERT_NE(aim.grab, EditorNoPick);
            ASSERT_TRUE(editor->Grab(aim.grab));
            editor->DragTo(aim.reach, false);
            editor->Release();
        }
    };
}

TEST_F(WorldEditorTest, AClickOnAMeshSelectsItsActor) {
    editor->PickAt(View * 0.5f);

    EXPECT_EQ(editor->Selection(), box);
    EXPECT_EQ(editor->State().selected, "box");
}

TEST_F(WorldEditorTest, AClickOnNothingClearsTheSelection) {
    editor->Select(box);
    editor->PickAt({5.0f, 5.0f});

    EXPECT_FALSE(editor->Selection().has_value());
}

TEST_F(WorldEditorTest, AClickOnAMarkerSelectsAnActorWithNoMesh) {
    editor->PickAt(pointOf(world.WorldPosition(sun)));

    EXPECT_EQ(editor->Selection(), sun);
}

TEST_F(WorldEditorTest, APressOutsideTheViewLeavesTheSelection) {
    editor->Select(box);
    editor->PickAt(View + Vec2{10.0f, 10.0f});

    EXPECT_EQ(editor->Selection(), box);
}

TEST_F(WorldEditorTest, NoViewTakesNoPress) {
    editor->Select(box);
    const auto aim = editor->Gizmo().moveX;
    editor->Update(std::nullopt);

    EXPECT_EQ(editor->Gizmo().moveX.grab, EditorNoPick);
    EXPECT_FALSE(editor->Grab(aim.grab));
    editor->PickAt(View * 0.5f);
    EXPECT_EQ(editor->Selection(), box);
}

TEST_F(WorldEditorTest, HidingTheViewLetsGoOfAHeldHandle) {
    editor->Select(box);
    ASSERT_TRUE(editor->Grab(editor->Gizmo().moveX.grab));

    editor->Update(std::nullopt);
    EXPECT_EQ(editor->Held(), GizmoHandle::None);
}

TEST_F(WorldEditorTest, ANameSelectsItsActor) {
    editor->State().selected = "sun";
    editor->Sync();

    EXPECT_EQ(editor->Selection(), sun);
}

TEST_F(WorldEditorTest, AnUnnamedActorGoesByItsFileId) {
    editor->Select(empty);

    EXPECT_EQ(
        editor->State().selected,
        std::format("actor-{}", world.FileId(empty))
    );
}

// a name is shown, not an identity: the port is told, and the actor itself
// still selects
TEST_F(WorldEditorTest, ARepeatedNameIsRefusedButItsActorsSelect) {
    const auto first = spawn("twin", Transform{});
    const auto second = spawn("twin", Transform{});
    world.Frame(0.0);

    editor->State().selected = "twin";
    editor->Sync();
    EXPECT_FALSE(editor->Selection().has_value());
    EXPECT_THAT(editor->State().status, HasSubstr("more than one"));

    editor->Select(second);
    EXPECT_EQ(editor->Selection(), second);
    editor->Select(first);
    EXPECT_EQ(editor->Selection(), first);
}

// a component with no properties, as MeshRowComponent, shows nothing
TEST_F(WorldEditorTest, TheTransformLeadsThenEachComponent) {
    editor->Select(sun);
    const auto& sections = editor->Sections();
    ASSERT_EQ(sections.size(), 2u);
    EXPECT_EQ(sections[0].label, "transform");
    EXPECT_EQ(sections[0].target.Read(), &world.LocalTransform(sun));
    EXPECT_EQ(sections[1].label, "LightComponent");
    EXPECT_EQ(sections[1].target.Read(), world.Get<LightComponent>(sun));

    editor->Select(box);
    EXPECT_EQ(editor->Sections().size(), 1u);
}

TEST_F(WorldEditorTest, ADestroyedSelectionClears) {
    editor->Select(box);
    world.Destroy(box);
    world.Frame(0.0);

    editor->Update(View);
    EXPECT_FALSE(editor->Selection().has_value());
}

TEST_F(WorldEditorTest, AnArrowMoveLandsInThePrimitiveRowAtTheNextFrame) {
    editor->Select(box);
    dragOnce(&GizmoAims::moveX);
    const auto row = *primitiveOfActor(world, box);
    EXPECT_EQ(scene.Primitives().GetRef(row).localToWorld[3].x, 0.0f);

    world.Frame(0.0);
    EXPECT_NEAR(scene.Primitives().GetRef(row).localToWorld[3].x, 1.0f, 1e-4f);
}

// a parent turned 90 degrees about Y and scaled 2: a meter along world X is
// half a meter along the parent's local Z
TEST_F(WorldEditorTest, AChildsWorldDragBecomesAParentLocalOffset) {
    const auto parent = world.Spawn(
        Transform{.rotation = rotateY(Pi / 2), .scale = {2.0f, 2.0f, 2.0f}}
    );
    world.SetParent(box, parent);
    world.Frame(0.0);

    editor->Select(box);
    dragOnce(&GizmoAims::moveX);
    expectNear(world.LocalTransform(box).position, {0.0f, 0.0f, 0.5f}, 1e-4f);

    world.Frame(0.0);
    expectNear(world.WorldPosition(box), {1.0f, 0.0f, 0.0f}, 1e-4f);
}

// the sun's light swings a quarter turn about world Y, from +Z toward +X
TEST_F(WorldEditorTest, TheRingTurnsAboutWorldY) {
    const auto row = world.Get<LightComponent>(sun)->row;
    const auto direction = scene.Lights().GetRef(row).direction;
    editor->Select(sun);
    dragOnce(&GizmoAims::ring);

    world.Frame(0.0);
    expectNear(
        scene.Lights().GetRef(row).direction,
        rotate(direction, rotateY(Pi / 2)),
        1e-4f
    );
}

// a parent turned 90 degrees about X: world Y is its local -Z, and the ring
// turns the child about world Y all the same
TEST_F(WorldEditorTest, TheRingInAParentTurnsAboutWorldY) {
    const auto turned = rotateX(Pi / 2);
    const auto parent = world.Spawn(Transform{.rotation = turned});
    world.SetParent(box, parent);
    world.Frame(0.0);

    editor->Select(box);
    dragOnce(&GizmoAims::ring);
    const auto rotation = quat(turned, world.LocalTransform(box).rotation);
    EXPECT_NEAR(
        std::abs(dot(rotation, quat(rotateY(Pi / 2), turned))),
        1.0f,
        1e-5f
    );
}

// an actor is a transform node, and an empty one carries its children
TEST_F(WorldEditorTest, AnEmptyActorTakesEveryHandle) {
    editor->Select(empty);

    EXPECT_NE(editor->Gizmo().ringX.grab, EditorNoPick);
    EXPECT_NE(editor->Gizmo().scaleX.grab, EditorNoPick);
}

// only the field the handle edits is the gizmo's
TEST_F(WorldEditorTest, AWriteToAnotherFieldSurvivesTheDrag) {
    editor->Select(box);
    const auto aim = editor->Gizmo().moveX;
    ASSERT_TRUE(editor->Grab(aim.grab));
    auto local = world.LocalTransform(box);
    local.scale = {2.0f, 2.0f, 2.0f};
    world.SetLocalTransform(box, local);
    editor->DragTo(aim.reach, false);

    expectNear(world.LocalTransform(box).position, {1.0f, 0.0f, 0.0f}, 1e-4f);
    EXPECT_EQ(world.LocalTransform(box).scale, (Vec3{2.0f, 2.0f, 2.0f}));
}

TEST_F(WorldEditorTest, CancelPutsTheGrabbedTransformBack) {
    editor->Select(box);
    const auto aim = editor->Gizmo().moveX;
    ASSERT_TRUE(editor->Grab(aim.grab));
    editor->DragTo(aim.reach, false);
    editor->Cancel();

    EXPECT_EQ(editor->Held(), GizmoHandle::None);
    EXPECT_EQ(world.LocalTransform(box).position, (Vec3{0.0f, 0.0f, 0.0f}));
}
