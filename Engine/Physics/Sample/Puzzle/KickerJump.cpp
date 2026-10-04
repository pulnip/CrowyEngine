#include "PuzzleContent.hpp"

namespace Crowy
{
    // The marble's drop down the ramp becomes speed; a raised kicker turns
    // that speed into a jump over the wall into the cup. Level, it meets the
    // wall's frictionless face and stops.
    Puzzle makeKickerJump() {
        using enum PieceRole;

        constexpr BodyPose GateRest{.position = {-1.1192741f, 1.68f, 0.0f}};
        constexpr BodyPose KickerRest{.position = {0.12f, 0.6f, 0.0f}};

        return Puzzle{
            .kind = PuzzleKind::KickerJump,
            .task = "raise the kicker, then open the gate",
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                // 40 degrees; its top runs 15 cm under the runway, so the
                // kink is one clean line
                PuzzleBody{
                    .name = "ramp",
                    .role = Wall,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.95f, 0.05f, 0.2f}},
                        .pose = BodyPose{
                            .position = {-0.72497493f, 1.0759279f, 0.0f},
                            .rotation = {0.0f, 0.0f, -0.34202014f, 0.93969262f},
                        },
                        .motion = BodyMotion::Static,
                        .friction = 0.6f,
                    },
                },
                staticBox(
                    "runway",
                    Wall,
                    {0.2f, 0.3f, 0.2f},
                    {0.02f, 0.3f, 0.0f},
                    0.6f
                ),
                PuzzleBody{
                    .name = "gate",
                    .role = Mover,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.05f, 0.15f, 0.2f}},
                        .pose = GateRest,
                        .motion = BodyMotion::Kinematic,
                        .friction = 0.0f,
                    },
                },
                // turns about its back end, which sinks under the runway
                PuzzleBody{
                    .name = "kicker",
                    .role = Mover,
                    .desc = BodyDesc{
                        .shape = CompoundShape{{
                            CompoundPart{
                                .halfExtent = {0.3f, 0.05f, 0.2f},
                                .pose = {.position = {0.2f, -0.05f, 0.0f}},
                            },
                        }},
                        .pose = KickerRest,
                        .motion = BodyMotion::Kinematic,
                        .friction = 0.6f,
                    },
                },
                // frictionless, so a marble meeting its face cannot climb it
                staticBox(
                    "frontWall",
                    Wall,
                    {0.02f, 0.385f, 0.25f},
                    {0.67f, 0.385f, 0.0f},
                    0.0f
                ),
                staticBox(
                    "cupFloor",
                    Goal,
                    {0.345f, 0.25f, 0.25f},
                    {1.035f, 0.25f, 0.0f}
                ),
                staticBox(
                    "backWall",
                    Wall,
                    {0.05f, 0.45f, 0.25f},
                    {1.43f, 0.45f, 0.0f}
                ),
                // against the gate, 1.64 m up the slope from its foot
                PuzzleBody{
                    .name = "marble",
                    .role = Payload,
                    .desc = BodyDesc{
                        .shape = SphereShape{0.1f},
                        .pose = BodyPose{
                            .position = {-1.2692741f, 1.7284602f, 0.0f},
                        },
                        .mass = 1.0f,
                        .friction = 0.6f,
                        .angularDamping = 0.05f,
                    },
                },
            },
            .zones = {
                PuzzleZone{
                    .name = "cup",
                    .center = {1.035f, 0.57f, 0.0f},
                    .halfExtent = {0.315f, 0.05f, 0.2f},
                },
            },
            // 30 degrees up, then the gate lifts
            .solution = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 4,
                        .keys = {
                            PoseKey{.tick = 10, .pose = KickerRest},
                            PoseKey{
                                .tick = 40,
                                .pose = {
                                    .position = KickerRest.position,
                                    .rotation =
                                        {0.0f, 0.0f, 0.25881905f, 0.96592583f},
                                },
                            },
                        },
                    },
                    PoseTrack{
                        .body = 3,
                        .keys = {
                            PoseKey{.tick = 60, .pose = GateRest},
                            PoseKey{
                                .tick = 64,
                                .pose = {
                                    .position = {-1.1192741f, 2.03f, 0.0f},
                                },
                                .easing = Easing::Linear,
                            },
                        },
                    },
                },
            },
            // the gate alone
            .control = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 3,
                        .keys = {
                            PoseKey{.tick = 60, .pose = GateRest},
                            PoseKey{
                                .tick = 64,
                                .pose = {
                                    .position = {-1.1192741f, 2.03f, 0.0f},
                                },
                                .easing = Easing::Linear,
                            },
                        },
                    },
                },
            },
            .goal = PuzzleGoal{.body = 8, .zone = 0},
        };
    }
}
