#pragma once

#include <string_view>

#include "PhysicsTypes.hpp"
#include "Primitives.hpp"
#include "Puzzle.hpp"

// one file of literals per puzzle, so tuning one recompiles only it
namespace Crowy
{
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

    Puzzle makeKickerJump();
    Puzzle makeTiltTray();
    Puzzle makeSwingCut();
    Puzzle makeBuoyPop();
}
