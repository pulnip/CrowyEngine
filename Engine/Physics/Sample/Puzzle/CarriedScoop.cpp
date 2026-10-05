#include "PuzzleContent.hpp"

#include <utility>

namespace Crowy
{
    // The ball rides a scoop open at the back; only its slope holds it
    // against the front wall. Started gently, the carry keeps it; started
    // with a jolt, its inertia rolls it out the back.
    Puzzle makeCarriedScoop() {
        using enum PieceRole;

        // floor 8 degrees down toward the front wall
        constexpr Vec4 Carry{0.0f, 0.0f, -0.069756474f, 0.99756405f};
        // 15 degrees down toward the open back
        constexpr Vec4 Pour{0.0f, 0.0f, 0.13052619f, 0.99144486f};
        constexpr BodyPose Rest{
            .position = {-1.2f, 1.0f, 0.0f},
            .rotation = Carry,
        };

        auto wall = [](Vec3 halfExtent, Vec3 position) {
            return CompoundPart{
                .halfExtent = halfExtent,
                .pose = {.position = position},
                .friction = 0.0f,
            };
        };
        auto at = [&](u64 tick, f32 x, Easing easing) {
            return PoseKey{
                .tick = tick,
                .pose = {.position = {x, 1.0f, 0.0f}, .rotation = Carry},
                .easing = easing,
            };
        };
        // over the basket: wait, then pour
        const PoseKeys delivery{
            at(150, 0.3f, Easing::Linear),
            at(210, 0.9f, Easing::EaseOut),
            at(240, 0.9f, Easing::Smooth),
            PoseKey{
                .tick = 280,
                .pose = {.position = {0.9f, 1.0f, 0.0f}, .rotation = Pour},
            },
        };
        auto carry = [&](PoseKeys keys) {
            keys.insert(keys.end(), delivery.begin(), delivery.end());
            return PuzzleScript{
                .tracks = {PoseTrack{.body = 1, .keys = std::move(keys)}},
            };
        };

        return Puzzle{
            .kind = PuzzleKind::CarriedScoop,
            .task = "carry the ball without a jolt",
            .bodies = {
                staticBox(
                    "floor",
                    Ground,
                    {1.5f, 0.25f, 1.5f},
                    {0.0f, -0.25f, 0.0f}
                ),
                // its origin is the floor's open back edge, the pour's pivot
                PuzzleBody{
                    .name = "scoop",
                    .role = Mover,
                    .desc = BodyDesc{
                        .shape = CompoundShape{{
                            CompoundPart{
                                .halfExtent = {0.2f, 0.03f, 0.2f},
                                .pose = {.position = {0.2f, -0.03f, 0.0f}},
                            },
                            wall({0.025f, 0.13f, 0.2f}, {0.375f, 0.07f, 0.0f}),
                            wall({0.2f, 0.09f, 0.025f}, {0.2f, 0.03f, -0.175f}),
                            wall({0.2f, 0.09f, 0.025f}, {0.2f, 0.03f, 0.175f}),
                        }},
                        .pose = Rest,
                        .motion = BodyMotion::Kinematic,
                        .friction = 0.5f,
                    },
                },
                staticBox(
                    "basketPad",
                    Goal,
                    {0.3f, 0.275f, 0.3f},
                    {0.7f, 0.275f, 0.0f}
                ),
                staticBox(
                    "basketBack",
                    Goal,
                    {0.025f, 0.4f, 0.3f},
                    {0.375f, 0.4f, 0.0f}
                ),
                staticBox(
                    "basketFront",
                    Goal,
                    {0.025f, 0.4f, 0.3f},
                    {1.025f, 0.4f, 0.0f}
                ),
                // against the front wall
                PuzzleBody{
                    .name = "ball",
                    .role = Payload,
                    .desc = BodyDesc{
                        .shape = SphereShape{0.1f},
                        .pose = BodyPose{
                            .position = {-0.93851567f, 1.0642335f, 0.0f},
                        },
                        .mass = 1.0f,
                        .friction = 0.5f,
                        .angularDamping = 0.5f,
                    },
                },
            },
            .zones = {
                PuzzleZone{
                    .name = "basket",
                    .center = {0.7f, 0.62f, 0.0f},
                    .halfExtent = {0.28f, 0.05f, 0.25f},
                },
            },
            // speeds up at 0.8 m/s^2, under the slope's hold of g tan 8
            .solution = carry({
                PoseKey{.tick = 30, .pose = Rest},
                at(120, -0.3f, Easing::EaseIn),
            }),
            // the same carry from tick 120, reached by a jolt to 1.2 m/s
            .control = carry({
                PoseKey{.tick = 75, .pose = Rest},
                at(120, -0.3f, Easing::Linear),
            }),
            .goal = PuzzleGoal{.body = 5, .zone = 0},
        };
    }
}
