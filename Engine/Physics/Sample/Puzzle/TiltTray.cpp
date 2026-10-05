#include "PuzzleContent.hpp"

namespace Crowy
{
    // The marble is never touched: tilting the tray sets which way gravity
    // pulls along its floor. Far side down, then right side down, then the
    // near side, and it rolls round the divider and out through the gap.
    Puzzle makeTiltTray() {
        using enum PieceRole;

        constexpr BodyPose Level{.position = {0.0f, 1.0f, 0.25f}};
        // far side 8 degrees down
        constexpr Vec4 FarDown{0.069756474f, 0.0f, 0.0f, 0.99756405f};
        // far and right sides 8 degrees down
        constexpr Vec4 FarRightDown{
            0.06958655f,
            0.0048659656f,
            -0.06958655f,
            0.99513403f,
        };
        // near side 10 degrees down, right side still 8
        constexpr Vec4 NearRightDown{
            -0.086943436f,
            -0.0060796773f,
            -0.069491029f,
            0.99376802f,
        };
        constexpr Vec4 RightDown{0.0f, 0.0f, -0.069756474f, 0.99756405f};

        auto rim = [](Vec3 halfExtent, Vec3 position) {
            return CompoundPart{
                .halfExtent = halfExtent,
                .pose = {.position = position},
                .friction = 0.0f,
            };
        };
        auto tilt = [&](u64 tick, Vec4 rotation) {
            return PoseKey{
                .tick = tick,
                .pose = {.position = Level.position, .rotation = rotation},
            };
        };

        return Puzzle{
            .kind = PuzzleKind::TiltTray,
            .task = "tilt the tray to roll the marble out",
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                // pivots about its floor's top centre; frictionless rims, so
                // the marble rolls along them
                PuzzleBody{
                    .name = "tray",
                    .role = Mover,
                    .desc = BodyDesc{
                        .shape = CompoundShape{{
                            CompoundPart{
                                .halfExtent = {0.9f, 0.05f, 0.9f},
                                .pose = {.position = {0.0f, -0.05f, 0.0f}},
                            },
                            rim({0.05f, 0.175f, 0.9f}, {-0.85f, 0.075f, 0.0f}),
                            rim({0.05f, 0.175f, 0.9f}, {0.85f, 0.075f, 0.0f}),
                            rim({0.8f, 0.175f, 0.05f}, {0.0f, 0.075f, 0.85f}),
                            // short of the right rim: the way out
                            rim({0.6f, 0.175f, 0.05f}, {-0.2f, 0.075f, -0.85f}),
                            // a passage at the far end
                            rim({0.05f, 0.175f, 0.55f}, {0.0f, 0.075f, -0.25f}),
                        }},
                        .pose = Level,
                        .motion = BodyMotion::Kinematic,
                        .friction = 0.5f,
                    },
                },
                staticBox(
                    "cupFloor",
                    Goal,
                    {0.35f, 0.15f, 0.35f},
                    {0.7f, 0.15f, -1.1f}
                ),
                staticBox(
                    "cupNear",
                    Goal,
                    {0.35f, 0.3f, 0.025f},
                    {0.7f, 0.3f, -1.425f}
                ),
                staticBox(
                    "cupLip",
                    Goal,
                    {0.35f, 0.225f, 0.025f},
                    {0.7f, 0.225f, -0.775f}
                ),
                // ends, so the spin it lands with cannot roll it out
                staticBox(
                    "cupLeft",
                    Goal,
                    {0.025f, 0.225f, 0.3f},
                    {0.375f, 0.225f, -1.1f}
                ),
                staticBox(
                    "cupRight",
                    Goal,
                    {0.025f, 0.225f, 0.3f},
                    {1.025f, 0.225f, -1.1f}
                ),
                // in the near left corner, left of the divider
                PuzzleBody{
                    .name = "marble",
                    .role = Payload,
                    .desc = BodyDesc{
                        .shape = SphereShape{0.1f},
                        .pose = BodyPose{.position = {-0.7f, 1.1f, -0.45f}},
                        .mass = 1.0f,
                        .friction = 0.5f,
                        .angularDamping = 0.5f,
                    },
                },
            },
            .zones = {
                PuzzleZone{
                    .name = "cup",
                    .center = {0.7f, 0.37f, -1.1f},
                    .halfExtent = {0.3f, 0.05f, 0.28f},
                },
            },
            // each tilt held 15 ticks past the marble's arrival
            .solution = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 1,
                        .keys = {
                            tilt(30, FarDown),
                            tilt(140, FarDown),
                            tilt(170, FarRightDown),
                            tilt(280, FarRightDown),
                            tilt(320, NearRightDown),
                        },
                    },
                },
            },
            // without the far tilt the marble never reaches the passage
            .control = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 1,
                        .keys = {
                            tilt(140, Level.rotation),
                            tilt(170, RightDown),
                            tilt(280, RightDown),
                            tilt(320, NearRightDown),
                        },
                    },
                },
            },
            .goal = PuzzleGoal{.body = 7, .zone = 0},
        };
    }
}
