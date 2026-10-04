#include "PuzzleContent.hpp"

namespace Crowy
{
    // the crate slides about 1 m: onto the plate after 0.45 m, short of the
    // stop; the raised gate lets the ball roll to the end wall
    Puzzle makePlateGate() {
        using enum PieceRole;

        return Puzzle{
            .kind = PuzzleKind::PlateGate,
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                staticBox(
                    "crateStop",
                    Wall,
                    {0.1f, 0.2f, 0.3f},
                    {0.5f, 0.2f, 0.6f}
                ),
                staticBox(
                    "endWall",
                    Wall,
                    {0.1f, 0.3f, 0.4f},
                    {1.4f, 0.3f, -0.6f}
                ),
                PuzzleBody{
                    .name = "gate",
                    .role = Gate,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.1f, 0.3f, 0.4f}},
                        .pose = BodyPose{.position = {0.3f, 0.31f, -0.6f}},
                        .motion = BodyMotion::Kinematic,
                    },
                },
                PuzzleBody{
                    .name = "crate",
                    .role = Crate,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.2f, 0.2f, 0.2f}},
                        .pose = BodyPose{.position = {-1.2f, 0.2f, 0.6f}},
                        .mass = 10.0f,
                        .friction = 0.5f,
                    },
                },
                puzzleBall({-1.2f, 0.15f, -0.6f}),
            },
            .zones = {
                PuzzleZone{
                    .name = "plate",
                    .role = ZoneRole::Plate,
                    .center = {-0.2f, 0.1f, 0.6f},
                    .halfExtent = {0.35f, 0.1f, 0.3f},
                },
                PuzzleZone{
                    .name = "goal",
                    .role = ZoneRole::Goal,
                    .center = {1.0f, 0.15f, -0.6f},
                    .halfExtent = {0.3f, 0.15f, 0.3f},
                },
            },
            // 50 levels of 1 cm lift the gate's bottom above the ball
            .plates = {
                PlateRule{
                    .zone = 0,
                    .minMass = 5.0f,
                    .gate = 3,
                    .rise = {0.0f, 0.01f, 0.0f},
                    .levels = 50,
                },
            },
            .solution = PuzzleScript{.impulses = {
                PuzzleInput{
                    .tick = 0,
                    .body = 4,
                    .impulse = {32.0f, 0.0f, 0.0f},
                },
                PuzzleInput{.tick = 120, .body = 5, .impulse = PuzzleBallPush},
            }},
            .control = PuzzleScript{.impulses = {
                PuzzleInput{.tick = 120, .body = 5, .impulse = PuzzleBallPush},
            }},
            .goal = PuzzleGoal{.body = 5, .zone = 1},
        };
    }
}
