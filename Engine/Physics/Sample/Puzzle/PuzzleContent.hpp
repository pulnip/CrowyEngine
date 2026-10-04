#pragma once

#include <string_view>

#include "PhysicsTypes.hpp"
#include "Primitives.hpp"
#include "Puzzle.hpp"

// one file of literals per puzzle, so tuning one recompiles only it
namespace Crowy
{
    inline constexpr f32 PuzzleBallRadius = 0.15f;
    // pushed along +x into every puzzle's lane
    inline constexpr Vec3 PuzzleBallPush{2.5f, 0.0f, 0.0f};

    inline constexpr PuzzleBody staticBox(
        std::string_view name,
        PieceRole role,
        Vec3 halfExtent,
        Vec3 position,
        f32 friction = 0.5f
    ) {
        return PuzzleBody{
            .name = name,
            .role = role,
            .desc = BodyDesc{
                .shape = BoxShape{halfExtent},
                .pose = BodyPose{.position = position},
                .motion = BodyMotion::Static,
                .friction = friction,
            },
        };
    }

    // angular damping, because a rolling ball has no rolling resistance
    inline constexpr PuzzleBody puzzleBall(Vec3 position) {
        return PuzzleBody{
            .name = "ball",
            .role = PieceRole::Payload,
            .desc = BodyDesc{
                .shape = SphereShape{PuzzleBallRadius},
                .pose = BodyPose{.position = position},
                .mass = 1.0f,
                .friction = 0.5f,
                .angularDamping = 1.0f,
            },
        };
    }

    Puzzle makePlateGate();
    Puzzle makeToppleBridge();
    Puzzle makeSwingDoor();
}
