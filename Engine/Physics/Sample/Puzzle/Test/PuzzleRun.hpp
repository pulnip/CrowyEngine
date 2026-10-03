#pragma once

#include <array>
#include <variant>
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

    inline void runTo(PuzzleSession& session, u64 tick) {
        while(session.GetStatus().tick < tick)
            session.Tick();
    }

    // the hash at tick 0 and after every tick up to the horizon
    inline TickHashes hashesToHorizon(PuzzleSession& session) {
        TickHashes hashes{session.GetStatus().hash};
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            hashes.push_back(session.GetStatus().hash);
        }

        return hashes;
    }

    inline AABB3D worldBoundsOf(const PhysicsWorld& world, BodyHandle body) {
        const auto shape = world.ShapeOf(body);
        const auto pose = world.PoseOf(body);
        // a sphere's bounds do not turn with it
        if(const auto* sphere = std::get_if<SphereShape>(&shape)) {
            return AABB3D{
                .center = pose.position,
                .halfScale = ones() * sphere->radius,
            };
        }

        return transformAABB3D(
            modelMat(pose.position, pose.rotation, ones()),
            AABB3D{
                .center = zeros(),
                .halfScale = std::get<BoxShape>(shape).halfExtent,
            }
        );
    }

    inline f32 distance(Vec3 a, Vec3 b) {
        return norm(a - b);
    }
}
