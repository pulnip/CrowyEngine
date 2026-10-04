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
        PuzzleKind::PlateGate,
        PuzzleKind::ToppleBridge,
        PuzzleKind::SwingDoor,
    };
    inline constexpr std::array AllPuzzleModes{
        PuzzleMode::Solution,
        PuzzleMode::Control,
    };
    // by PuzzleKind: every body, and those that move
    inline constexpr std::array<usize, 3> PuzzleBodyCounts{6, 7, 6};
    inline constexpr std::array<usize, 3> PuzzleMovingCounts{3, 2, 2};

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
