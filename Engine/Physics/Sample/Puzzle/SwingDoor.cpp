#include "PuzzleContent.hpp"

namespace Crowy
{
    // A door hinged on a vertical axis blocks the corridor; a push at its
    // free edge swings it open against its friction to the limit, and the
    // ball rolls through. The ball alone pushes ten times under that
    // friction, so without the push the door holds.
    Puzzle makeSwingDoor() {
        using enum PieceRole;

        return Puzzle{
            .kind = PuzzleKind::SwingDoor,
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                staticBox(
                    "wallNear",
                    Wall,
                    {1.5f, 0.3f, 0.05f},
                    {0.0f, 0.3f, -0.55f}
                ),
                staticBox(
                    "wallFar",
                    Wall,
                    {1.5f, 0.3f, 0.05f},
                    {0.0f, 0.3f, 0.55f}
                ),
                staticBox(
                    "endWall",
                    Wall,
                    {0.1f, 0.3f, 0.5f},
                    {1.4f, 0.3f, 0.0f}
                ),
                PuzzleBody{
                    .name = "door",
                    .role = Door,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.06f, 0.35f, 0.4f}},
                        .pose = BodyPose{.position = {0.0f, 0.4f, 0.0f}},
                        .mass = 10.0f,
                        .friction = 0.5f,
                    },
                },
                puzzleBall({-1.2f, 0.15f, 0.0f}),
            },
            // opening turns the free edge toward +x, away from the ball
            .hinges = {
                PuzzleHinge{
                    .body = 4,
                    .desc = HingeDesc{
                        .pivot = {0.0f, 0.4f, -0.4f},
                        .axis = unitY(),
                        .normal = unitX(),
                        .minAngle = 0.0f,
                        .maxAngle = 1.5f,
                        .maxFrictionTorque = 20.0f,
                    },
                },
            },
            .zones = {
                PuzzleZone{
                    .name = "goal",
                    .role = ZoneRole::Goal,
                    .center = {1.1f, 0.15f, 0.0f},
                    .halfExtent = {0.2f, 0.15f, 0.25f},
                },
            },
            .solution = {
                PuzzleInput{
                    .tick = 0,
                    .body = 4,
                    .impulse = {22.0f, 0.0f, 0.0f},
                    .point = Vec3{0.0f, 0.4f, 0.38f},
                },
                PuzzleInput{.tick = 120, .body = 5, .impulse = PuzzleBallPush},
            },
            .control = {
                PuzzleInput{.tick = 120, .body = 5, .impulse = PuzzleBallPush},
            },
            .goal = PuzzleGoal{.body = 5, .zone = 0},
        };
    }
}
