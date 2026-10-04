#pragma once

#include <array>
#include <vector>

#include "Geometry/Overlap3D.hpp"
#include "LinearAlgebra.hpp"
#include "PhysicsWorld.hpp"
#include "Puzzle.hpp"
#include "PuzzleSession.hpp"

namespace Crowy
{
    using TickHashes = std::vector<u64>;

    inline constexpr std::array AllPuzzleKinds{
        PuzzleKind::KickerJump,
        PuzzleKind::SwingCut,
        PuzzleKind::BuoyPop,
    };
    inline constexpr std::array AllPuzzleModes{
        PuzzleMode::Solution,
        PuzzleMode::Control,
    };
    static_assert(
        AllPuzzleKinds.size() == EnumTraits<PuzzleKind>::entries.size()
    );
    // by PuzzleKind: every body, those that move, and the hinges
    inline constexpr std::array<usize, 3> PuzzleBodyCounts{9, 9, 12};
    inline constexpr std::array<usize, 3> PuzzleMovingCounts{3, 1, 2};
    inline constexpr std::array<usize, 3> PuzzleHingeCounts{0, 1, 0};

    inline constexpr usize indexOf(PuzzleKind kind) {
        return static_cast<usize>(kind);
    }

    inline f32 distance(Vec3 a, Vec3 b) {
        return norm(a - b);
    }

    void runTo(PuzzleSession& session, u64 tick);
    // the hash at tick 0 and after every tick up to the horizon
    TickHashes hashesToHorizon(PuzzleSession& session);
    // a sphere's bounds do not turn with it; a compound's unite its parts
    AABB3D boundsOf(const BodyShape& shape, const BodyPose& pose);
    AABB3D worldBoundsOf(const PhysicsWorld& world, BodyHandle body);
}
