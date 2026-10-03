#include "PuzzleRun.hpp"

#include <variant>

namespace Crowy
{
    void runTo(PuzzleSession& session, u64 tick) {
        while(session.GetStatus().tick < tick)
            session.Tick();
    }

    TickHashes hashesToHorizon(PuzzleSession& session) {
        TickHashes hashes{session.GetStatus().hash};
        while(session.GetStatus().tick < PuzzleHorizon) {
            session.Tick();
            hashes.push_back(session.GetStatus().hash);
        }

        return hashes;
    }

    AABB3D worldBoundsOf(const PhysicsWorld& world, BodyHandle body) {
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
}
