#include "PuzzleContent.hpp"

namespace Crowy
{
    // pushed at its center of percussion, 0.95 m above the pivot edge, the
    // slab tips over without the edge needing friction and bridges the gap
    Puzzle makeToppleBridge() {
        using enum PieceRole;

        return Puzzle{
            .kind = PuzzleKind::ToppleBridge,
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f},
                    0.8f
                ),
                staticBox(
                    "nearPlatform",
                    Ground,
                    {0.55f, 0.25f, 0.5f},
                    {-0.95f, 0.25f, 0.0f},
                    0.8f
                ),
                staticBox(
                    "farPlatform",
                    Ground,
                    {0.55f, 0.25f, 0.5f},
                    {0.95f, 0.25f, 0.0f},
                    0.8f
                ),
                staticBox(
                    "pad",
                    Ground,
                    {0.35f, 0.1f, 0.5f},
                    {-1.15f, 0.6f, 0.0f},
                    0.8f
                ),
                staticBox(
                    "backstop",
                    Wall,
                    {0.075f, 0.15f, 0.5f},
                    {1.425f, 0.65f, 0.0f}
                ),
                PuzzleBody{
                    .name = "slab",
                    .role = Slab,
                    .desc = BodyDesc{
                        .shape = BoxShape{{0.1f, 0.7f, 0.3f}},
                        .pose = BodyPose{.position = {0.8f, 1.2f, 0.0f}},
                        .mass = 20.0f,
                        .friction = 0.8f,
                    },
                },
                puzzleBall({-1.3f, 0.85f, 0.0f}),
            },
            .zones = {
                PuzzleZone{
                    .name = "goal",
                    .role = ZoneRole::Goal,
                    .center = {1.15f, 0.65f, 0.0f},
                    .halfExtent = {0.2f, 0.15f, 0.3f},
                },
            },
            .solution = PuzzleScript{.impulses = {
                PuzzleInput{
                    .tick = 0,
                    .body = 5,
                    .impulse = {-20.0f, 0.0f, 0.0f},
                    .point = Vec3{0.8f, 1.45f, 0.0f},
                },
                PuzzleInput{.tick = 120, .body = 6, .impulse = PuzzleBallPush},
            }},
            .control = PuzzleScript{.impulses = {
                PuzzleInput{.tick = 120, .body = 6, .impulse = PuzzleBallPush},
            }},
            .goal = PuzzleGoal{.body = 6, .zone = 0},
        };
    }
}
