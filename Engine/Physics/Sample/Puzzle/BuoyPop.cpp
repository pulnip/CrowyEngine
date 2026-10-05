#include "PuzzleContent.hpp"

namespace Crowy
{
    // Held deep, the buoy stores the water's lift; let go sideways, it shoots
    // up through the hoop. Pushed only a little, it bobs back and stops short.
    Puzzle makeBuoyPop() {
        using enum PieceRole;

        constexpr Vec3 PlungerRest{-0.3f, 1.505154f, 0.0f};

        return Puzzle{
            .kind = PuzzleKind::BuoyPop,
            .task = "hold the buoy deep, then let go",
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                staticBox(
                    "tankFloor",
                    Wall,
                    {0.9f, 0.1f, 0.6f},
                    {0.0f, 0.1f, 0.0f}
                ),
                staticBox(
                    "tankLeft",
                    Wall,
                    {0.05f, 0.65f, 0.6f},
                    {-0.85f, 0.65f, 0.0f}
                ),
                staticBox(
                    "tankRight",
                    Wall,
                    {0.05f, 0.65f, 0.6f},
                    {0.85f, 0.65f, 0.0f}
                ),
                staticBox(
                    "tankNear",
                    Wall,
                    {0.8f, 0.65f, 0.05f},
                    {0.0f, 0.65f, -0.55f}
                ),
                staticBox(
                    "tankFar",
                    Wall,
                    {0.8f, 0.65f, 0.05f},
                    {0.0f, 0.65f, 0.55f}
                ),
                // floating: a 0.01 m^3 cap under water carries its 10 kg
                PuzzleBody{
                    .name = "buoy",
                    .role = Payload,
                    .desc = BodyDesc{
                        .shape = SphereShape{0.2f},
                        .pose = BodyPose{.position = {-0.3f, 1.255154f, 0.0f}},
                        .mass = 10.0f,
                        .friction = 0.5f,
                    },
                },
                // a centimeter above the buoy
                PuzzleBody{
                    .name = "plunger",
                    .role = Mover,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.3f, 0.04f, 0.3f}},
                        .pose = BodyPose{.position = PlungerRest},
                        .motion = BodyMotion::Kinematic,
                        .friction = 0.0f,
                    },
                },
                // a 0.6 m square opening over the buoy
                staticBox(
                    "hoopLeft",
                    Goal,
                    {0.05f, 0.05f, 0.4f},
                    {-0.65f, 1.65f, 0.0f}
                ),
                staticBox(
                    "hoopRight",
                    Goal,
                    {0.05f, 0.05f, 0.4f},
                    {0.05f, 1.65f, 0.0f}
                ),
                staticBox(
                    "hoopNear",
                    Goal,
                    {0.3f, 0.05f, 0.05f},
                    {-0.3f, 1.65f, -0.35f}
                ),
                staticBox(
                    "hoopFar",
                    Goal,
                    {0.3f, 0.05f, 0.05f},
                    {-0.3f, 1.65f, 0.35f}
                ),
            },
            .zones = {
                PuzzleZone{
                    .name = "aboveHoop",
                    .center = {-0.3f, 2.15f, 0.0f},
                    .halfExtent = {0.25f, 0.05f, 0.25f},
                },
            },
            // the surface at 1.2; a low drag lets depth count
            .waters = {
                WaterDesc{
                    .center = {0.0f, 0.7f, 0.0f},
                    .halfExtent = {0.8f, 0.5f, 0.5f},
                    .density = 1000.0f,
                    .linearDrag = 0.05f,
                    .angularDrag = 0.01f,
                },
            },
            // 0.6 m deep, held, then away faster than the buoy can follow
            .solution = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 7,
                        .keys = {
                            PoseKey{
                                .tick = 60,
                                .pose = {.position = {-0.3f, 0.84f, 0.0f}},
                            },
                            PoseKey{
                                .tick = 90,
                                .pose = {.position = {-0.3f, 0.84f, 0.0f}},
                            },
                            PoseKey{
                                .tick = 94,
                                .pose = {.position = {0.4f, 0.94f, 0.0f}},
                                .easing = Easing::Linear,
                            },
                        },
                    },
                },
            },
            // 0.15 m deep: the buoy's top never goes under
            .control = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 7,
                        .keys = {
                            PoseKey{
                                .tick = 60,
                                .pose = {.position = {-0.3f, 1.29f, 0.0f}},
                            },
                            PoseKey{
                                .tick = 90,
                                .pose = {.position = {-0.3f, 1.29f, 0.0f}},
                            },
                            PoseKey{
                                .tick = 94,
                                .pose = {.position = {0.4f, 1.39f, 0.0f}},
                                .easing = Easing::Linear,
                            },
                        },
                    },
                },
            },
            .goal = PuzzleGoal{.body = 6, .zone = 0},
        };
    }
}
