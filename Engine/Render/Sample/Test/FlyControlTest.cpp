#include <algorithm>
#include <vector>

#include <gtest/gtest.h>

#include "CameraComponent.hpp"
#include "FlyCamera.hpp"
#include "FlyControl.hpp"
#include "InputProvider.hpp"
#include "SnapshotCamera.hpp"
#include "World.hpp"

using namespace Crowy;

namespace
{
    // input played back from a script: the keys held and where the mouse is
    class ScriptedInput final: public InputProvider {
    private:
        std::vector<KeyCode> held;
        bool looking = false;

    public:
        bool IsKeyDown(KeyCode key) const noexcept override {
            return std::ranges::find(held, key) != held.end();
        }
        bool IsKeyDown(MouseButton button) const noexcept override {
            return looking && button == MouseButton::RButton;
        }

        void Set(std::vector<KeyCode> keys, bool look, Vec2 mouse) {
            held = std::move(keys);
            looking = look;
            SetCurrentMousePos(mouse);
        }
    };

    struct Step {
        std::vector<KeyCode> keys;
        bool look = false;
        Vec2 mouse;
    };
}

// The goldens never move a camera, so this is what holds a camera actor
// flown by the shared fly functions to the bits of a FlyCamera flown alike.
TEST(FlyControl, AFlownCameraActorViewsAsAFlyCameraDoes) {
    const FlyCamera::Config config{
        .position = {0.0f, 1.7f, -5.0f},
        .yaw = 0.0f,
        .pitch = 0.1f,
        .fovY = 1.0471976f,
        .nearZ = 0.05f,
        .farZ = 100.0f,
        .moveSpeed = 5.0f
    };
    FlyCamera flyCamera(config);

    SnapshotCamera snapshotCamera;
    World world;
    world.Provide(snapshotCamera.Snapshot());
    auto yaw = config.yaw;
    auto pitch = config.pitch;
    const auto actor = world.Spawn(
        Transform{
            .position = config.position,
            .rotation = flyRotation(yaw, pitch)
        }
    );
    world.Add(
        actor,
        CameraComponent{
            .fovY = config.fovY,
            .nearZ = config.nearZ,
            .farZ = config.farZ
        }
    );
    world.Frame(0.0);

    using enum KeyCode;
    const std::vector<Step> script{
        {{}, false, {100.0f, 100.0f}},
        {{W}, false, {100.0f, 100.0f}},
        {{W, D}, true, {140.0f, 90.0f}},
        {{}, true, {170.0f, 130.0f}},
        {{S, Space}, false, {170.0f, 130.0f}},
        {{A, Shift, W}, true, {120.0f, 60.0f}},
        {{}, false, {120.0f, 60.0f}},
    };
    // a counted frame's step, then a wall clock's
    const std::vector<f64> steps{1.0 / 60.0, 0.0123456789};

    ScriptedInput input;
    for(usize frame = 0; frame < 30; ++frame) {
        const auto& step = script[frame % script.size()];
        const auto seconds = steps[frame % steps.size()];
        input.Set(step.keys, step.look, step.mouse);

        flyCamera.ProcessInput(input);
        flyCamera.Update(seconds);

        const auto turned = flyLook(input, config.lookSensitivity, yaw, pitch);
        const auto moveInput = flyMoveInput(input);
        if(turned || moveInput != zeros()) {
            world.SetLocalTransform(
                actor,
                flyStep(
                    world.LocalTransform(actor),
                    yaw,
                    pitch,
                    moveInput,
                    config.moveSpeed,
                    seconds
                )
            );
        }
        world.Frame(seconds);

        ASSERT_EQ(snapshotCamera.Position(), flyCamera.Position())
            << "frame " << frame;
        ASSERT_EQ(snapshotCamera.View(), flyCamera.View()) << "frame " << frame;
        ASSERT_EQ(
            snapshotCamera.ViewProj(16.0f / 9.0f),
            flyCamera.ViewProj(16.0f / 9.0f)
        ) << "frame "
          << frame;
    }

    // the script walked and turned, so the frames above compared motion
    EXPECT_NE(flyCamera.Position(), config.position);
    EXPECT_NE(yaw, config.yaw);
    EXPECT_NE(pitch, config.pitch);
}
