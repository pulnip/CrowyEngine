#include "PuzzleContent.hpp"

namespace Crowy
{
    // A weight dropped on the short arm of a lever throws the cube on the
    // long arm. Dropped from high, the throw reaches the ledge; let down
    // from where it waits, it only tosses the cube.
    Puzzle makeLeverCatapult() {
        using enum PieceRole;

        // every pose on the lever: 20 degrees about z, short arm up
        constexpr Vec4 Tilt{0.0f, 0.0f, 0.17364818f, 0.98480775f};
        constexpr BodyPose ShelfRest{
            .position = {0.66259866f, 1.20122845f, 0.0f},
            .rotation = Tilt,
        };

        auto shelfAt = [&](u64 tick, Vec3 position, Easing easing) {
            return PoseKey{
                .tick = tick,
                .pose = {.position = position, .rotation = Tilt},
                .easing = easing,
            };
        };

        return Puzzle{
            .kind = PuzzleKind::LeverCatapult,
            .task = "raise the weight high, then drop it",
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                // 2.6 m, its 1.75 m long arm toward -x; thick enough that
                // the weight's 9 cm a tick cannot pass through
                PuzzleBody{
                    .name = "plank",
                    .role = Lever,
                    .desc = BodyDesc{
                        .shape = BoxShape{{1.3f, 0.06f, 0.2f}},
                        .pose = BodyPose{
                            .position = {-0.12286168f, 0.59609094f, 0.0f},
                            .rotation = Tilt,
                        },
                        .mass = 4.0f,
                        .friction = 0.8f,
                        .linearDamping = 0.0f,
                        .angularDamping = 0.0f,
                    },
                },
                // 1.5 m out on the long arm, well inside its rounded end
                PuzzleBody{
                    .name = "cube",
                    .role = Payload,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.075f, 0.075f, 0.075f}},
                        .pose = BodyPose{
                            .position = {-1.15571165f, 0.36382829f, 0.0f},
                            .rotation = Tilt,
                        },
                        .mass = 1.0f,
                        .friction = 0.8f,
                    },
                },
                // landed, it balances the long arm and the cube at rest and
                // outweighs them as the lever turns
                PuzzleBody{
                    .name = "weight",
                    .role = Weight,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.1f, 0.1f, 0.1f}},
                        .pose = BodyPose{
                            .position = {0.61471584f, 1.33278542f, 0.0f},
                            .rotation = Tilt,
                        },
                        .mass = 10.0f,
                        .friction = 0.8f,
                        .linearDamping = 0.0f,
                        .angularDamping = 0.0f,
                    },
                },
                PuzzleBody{
                    .name = "shelf",
                    .role = Mover,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.15f, 0.04f, 0.15f}},
                        .pose = ShelfRest,
                        .motion = BodyMotion::Kinematic,
                        .friction = 0.8f,
                    },
                },
                staticBox(
                    "ledge",
                    Goal,
                    {0.525f, 0.05f, 0.25f},
                    {-0.175f, 1.75f, 0.0f}
                ),
                staticBox(
                    "ledgeWall",
                    Goal,
                    {0.025f, 0.2f, 0.25f},
                    {0.375f, 1.9f, 0.0f}
                ),
                // the stand under the pivot, clear of the plank's width
                staticBox(
                    "standNear",
                    Wall,
                    {0.04f, 0.375f, 0.04f},
                    {0.3f, 0.375f, -0.26f}
                ),
                staticBox(
                    "standFar",
                    Wall,
                    {0.04f, 0.375f, 0.04f},
                    {0.3f, 0.375f, 0.26f}
                ),
                // the ledge's board, behind the plank and the cube's flight
                staticBox(
                    "ledgeBoard",
                    Wall,
                    {0.45f, 0.875f, 0.05f},
                    {-0.25f, 0.875f, 0.3f}
                ),
            },
            // stops with the long arm 20 degrees up
            .hinges = {
                PuzzleHinge{
                    .body = 1,
                    .desc = HingeDesc{
                        .pivot = {0.3f, 0.75f, 0.0f},
                        .axis = {0.0f, 0.0f, -1.0f},
                        .normal = unitX(),
                        .minAngle = 0.0f,
                        .maxAngle = 0.6981317f,
                    },
                },
            },
            .zones = {
                PuzzleZone{
                    .name = "ledge",
                    .center = {-0.17f, 1.885f, 0.0f},
                    .halfExtent = {0.5f, 0.065f, 0.25f},
                },
            },
            // 1.2 m up, held, then out from under faster than it falls
            .solution = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 4,
                        .keys = {
                            shelfAt(
                                90,
                                {0.66259866f, 2.40122845f, 0.0f},
                                Easing::Smooth
                            ),
                            shelfAt(
                                120,
                                {0.66259866f, 2.40122845f, 0.0f},
                                Easing::Smooth
                            ),
                            shelfAt(
                                122,
                                {0.70364107f, 2.28846534f, 0.0f},
                                Easing::Linear
                            ),
                            shelfAt(
                                125,
                                {0.70364107f, 2.28846534f, 0.5f},
                                Easing::Linear
                            ),
                        },
                    },
                },
            },
            // the same release from where it waits: a 0.28 m drop
            .control = PuzzleScript{
                .tracks = {
                    PoseTrack{
                        .body = 4,
                        .keys = {
                            PoseKey{.tick = 120, .pose = ShelfRest},
                            shelfAt(
                                122,
                                {0.70364107f, 1.08846534f, 0.0f},
                                Easing::Linear
                            ),
                            shelfAt(
                                125,
                                {0.70364107f, 1.08846534f, 0.5f},
                                Easing::Linear
                            ),
                        },
                    },
                },
            },
            .goal = PuzzleGoal{.body = 2, .zone = 0},
        };
    }
}
