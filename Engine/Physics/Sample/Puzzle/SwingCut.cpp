#include "PuzzleContent.hpp"

namespace Crowy
{
    // A cut bob flies off along its tangent. Past the bottom it climbs over
    // the front wall into the basket; at the bottom it leaves level and hits
    // the wall's face.
    Puzzle makeSwingCut() {
        using enum PieceRole;

        return Puzzle{
            .kind = PuzzleKind::SwingCut,
            .task = "cut the rope on the way up",
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                staticBox(
                    "frontWall",
                    Wall,
                    {0.025f, 0.725f, 0.3f},
                    {0.925f, 0.725f, 0.0f}
                ),
                staticBox(
                    "basketPad",
                    Goal,
                    {0.225f, 0.575f, 0.3f},
                    {1.175f, 0.575f, 0.0f}
                ),
                staticBox(
                    "backWall",
                    Wall,
                    {0.025f, 0.95f, 0.3f},
                    {1.425f, 0.95f, 0.0f}
                ),
                // a bob that hits the front wall rolls back this far
                staticBox(
                    "curb",
                    Wall,
                    {0.05f, 0.1f, 0.3f},
                    {-1.45f, 0.1f, 0.0f}
                ),
                // the frame holding the pivot stands clear of the bob's plane
                staticBox(
                    "postNear",
                    Wall,
                    {0.04f, 1.64f, 0.04f},
                    {-0.25f, 1.64f, -0.4f}
                ),
                staticBox(
                    "postFar",
                    Wall,
                    {0.04f, 1.64f, 0.04f},
                    {-0.25f, 1.64f, 0.4f}
                ),
                staticBox(
                    "crossbar",
                    Wall,
                    {0.04f, 0.04f, 0.44f},
                    {-0.25f, 3.24f, 0.0f}
                ),
                // 50 degrees left of the pivot on a 1.4 m rod; no linear
                // damping keeps the swing's timing analytic
                PuzzleBody{
                    .name = "bob",
                    .role = Payload,
                    .desc = BodyDesc{
                        .shape = SphereShape{0.15f},
                        .pose = BodyPose{
                            .position = {-1.3224622f, 2.3000973f, 0.0f},
                        },
                        .mass = 5.0f,
                        .friction = 0.5f,
                        .linearDamping = 0.0f,
                        .angularDamping = 0.5f,
                    },
                },
            },
            .hinges = {
                PuzzleHinge{
                    .body = 8,
                    .desc = HingeDesc{
                        .pivot = {-0.25f, 3.2f, 0.0f},
                        .axis = unitZ(),
                        .normal = unitX(),
                    },
                    .tethered = true,
                },
            },
            // on the pad's top, as deep as a resting bob's radius
            .zones = {
                PuzzleZone{
                    .name = "basket",
                    .center = {1.175f, 1.225f, 0.0f},
                    .halfExtent = {0.2f, 0.075f, 0.25f},
                },
            },
            .solution = PuzzleScript{
                .releases = {HingeRelease{.tick = 50, .hinge = 0}},
            },
            // the bottom of the swing
            .control = PuzzleScript{
                .releases = {HingeRelease{.tick = 37, .hinge = 0}},
            },
            .goal = PuzzleGoal{.body = 8, .zone = 0},
        };
    }
}
